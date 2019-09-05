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

package org.testKitGen;

public class Options {
	private static String spec = "";
	private static String jdkVersion = "";
	private static String impl = "";
	// TODO: simpilify the dir after testing
	private static String projectRootDir = System.getProperty("user.dir") + "/scripts/testKitGen/../../..";
	private static String buildList = "";
	private static String iterations = "";
	private static String testFlag = "";

	private static final String usage = "Usage:\n"
			+ "    java TestKitGen --spec=[linux_x86-64] --jdkVersion=[8|9|...] --impl=[openj9|ibm|hotspot|sap] [options]\n\n"
			+ "Options:\n" + "    --spec=<spec>           Spec that the build will run on\n"
			+ "    --jdkVersion=<version>    JDK version that the build will run on, e.g. 8, 9, 10, etc.\n"
			+ "    --impl=<implementation>   Java implementation, e.g. openj9, ibm, hotspot, sap\n"
			+ "    --projectRootDir=<path>   Root path for searching playlist.xml\n"
			+ "                              Defaults to openj9/test\n"
			+ "                              Defaults to projectRootDir/TestConfig/resources\n"
			+ "    --buildList=<paths>       Comma separated project paths (relative to projectRootDir) to search for playlist.xml\n"
			+ "                              Defaults to projectRootDir\n"
			+ "    --iterations=<number>     Repeatedly generate test command based on iteration number\n"
			+ "                              Defaults to 1\n"
			+ "    --testFlag=<string>       Comma separated string to specify different test flags\n"
			+ "                              Defaults to \"\"\n";

	private Options() {
	}

	public static String getSpec() {
		return spec;
	}

	public static String getJdkVersion() {
		return jdkVersion;
	}

	public static String getImpl() {
		return impl;
	}

	public static String getProjectRootDir() {
		return projectRootDir;
	}

	public static String getBuildList() {
		return buildList;
	}

	public static String getIterations() {
		return iterations;
	}

	public static String getTestFlag() {
		return testFlag;
	}

	public static void parse(String[] args) {
		for (int i = 0; i < args.length; i++) {
			String arg = args[i];
			String arglc = arg.toLowerCase();
			if (arglc.startsWith("--spec=")) {
				spec = arglc.substring(arg.indexOf("=") + 1);
			} else if (arglc.startsWith("--jdkversion=")) {
				jdkVersion = arglc.substring(arg.indexOf("=") + 1);
			} else if (arglc.startsWith("--impl=")) {
				impl = arglc.substring(arg.indexOf("=") + 1);
			} else if (arglc.startsWith("--projectrootdir=")) {
				// projectRootDir is case sensitive
				projectRootDir = arg.substring(arg.indexOf("=") + 1);
			} else if (arglc.startsWith("--buildlist=")) {
				// buildList is case sensitive
				buildList = arg.substring(arg.indexOf("=") + 1);
			} else if (arglc.startsWith("--iterations=")) {
				iterations = arglc.substring(arg.indexOf("=") + 1);
			} else if (arglc.startsWith("--testflag=")) {
				testFlag = arglc.substring(arg.indexOf("=") + 1);
			} else {
				System.err.println("Invalid option " + args[i]);
				System.err.println(usage);
				System.exit(1);
			}
		}
	}
}