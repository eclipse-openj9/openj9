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
package j9vm.test.ddrext.util;

import j9vm.test.ddrext.Constants;

import java.net.URL;
import org.testng.log4testng.Logger;

public class GenerateCore {

	private static Logger log = Logger.getLogger(GenerateCore.class);

	/**
	 * @param args
	 *            args[0] -- Java home e.g./bluebird/builds/bld_87583/SDK
	 *            args[1] -- Optional, core dump location. If not specify, store
	 *            the core dump to the default location e.g. under current build
	 *            folder
	 */
	public static void main(String[] args) {

		if (args.length == 2 || args.length == 1) {
			String javaHome = args[0];
			String dumpLocation = null;
			// find j9vm_SE60.jar path
			String j9vmSE60Jar = findJ9vmSE60Jar(Constants.CORE_GEN_CLASS);

			String os = System.getProperty("os.name");
			log.debug("Run Test on " + os);

			// find platform name e.g. ap3270 in java home path
			// e.g./bluebird/builds/bld_87722/sdk/ap3270/jre/bin
			log.debug("Java Home: " + javaHome);
			String platform = javaHome.substring(javaHome.indexOf("sdk") + 4,
					javaHome.indexOf("jre") - 1);

			if (args.length == 2) {
				dumpLocation = args[1] + Constants.FS + platform;
			} else {
				String beforeSDK = javaHome.substring(0,
						javaHome.indexOf("sdk"));
				String coreFolder = Constants.CORE_DUMP_FOLDER;
				log.debug("beforeSDK:  " + beforeSDK + ",core folder:"
						+ coreFolder + ", platform:" + platform);
				dumpLocation = beforeSDK + coreFolder + Constants.FS + platform;
			}

			log.debug("dumpLocation: " + dumpLocation);
			log.info("Start to Generate Core using: " + javaHome + " and "
					+ Constants.CORE_GEN_CLASS + " in " + dumpLocation);
			CoreDumpGenerator dumpGenerator = new CoreDumpGenerator(javaHome,
					j9vmSE60Jar);
			String generatedCoreFile = dumpGenerator.generateDump(dumpLocation);
			if (generatedCoreFile == null) {
				log.error("FAIL! No dynamic dump created");
			} else {
				log.info("SUCCESS! Ddynamic dump is created:"
						+ generatedCoreFile);
			}
		} else {
			log.error("Please provide java_home and the locatin to store the generated core dump(option, the default location is java_home\\[platform])");
		}
	}

	// provide class name, find the jar location
	public static String findJ9vmSE60Jar(String aClassInJar) {
		URL location = aClassInJar.getClass()
				.getResource(
						'/' + CoreDumpUtil.class.getName().replace(".", "/")
								+ ".class");
		String jarPath = location.getPath();
		log.info("Jar path:" + jarPath);
		return jarPath.substring("file:".length(), jarPath.lastIndexOf("!"));
	}
}
