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
package j9vm.test.ddrext.util;

import j9vm.test.ddrext.Constants;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;

import org.testng.log4testng.Logger;

public class CoreDumpGenerator {

	private String command = null;
	private String generatedCoreFile = null;
	private static Logger log = Logger.getLogger(CoreDumpGenerator.class);
	private String dumpName = null;

	public CoreDumpGenerator(String javaHome, String j9vmSE60Jar) {
		setDumpName(javaHome);
		command = javaHome + Constants.FS + "java "
				+ Constants.CORE_GEN_VM_XDUMP_OPTIONS + dumpName
				+ Constants.CORE_GEN_VM_OPTIONS + " -cp " + j9vmSE60Jar
				+ " j9vm.test.ddrext.util.CoreDumpUtil";
	}

	public String generateDump() {
		String currentDir = System.getProperty("user.dir");
		String dumpFileDir = currentDir + Constants.FS + dumpName;

		// check if the dump file with the same name exists. If it does, delete
		// it.
		if (new File(dumpFileDir).exists()) {
			new File(dumpFileDir).delete();
			log.info("Deleted dump file :" + dumpFileDir);
		}
		log.debug("Current Dir: " + currentDir);
		log.debug("Command: " + command);
		try {
			runCommand(command);

		} catch (Exception e) {
			log.error("Not able to execute command " + command);
			e.printStackTrace();
		}

		// check if the dump file is actually generated
		if (new File(dumpFileDir).exists()) {
			return dumpFileDir;
		} else {
			return null;
		}
	}

	public String generateDump(String dumpLocation) {

		String os = System.getProperty("os.name");
		String currentDir = System.getProperty("user.dir");
		log.debug("Current Dir: " + currentDir);

		try {
			if (os.contains("windows") || os.contains("Windows")) {
				// TO_DO:Change drive from C: to L:
				// String drive = dumpLocation.substring(0,
				// dumpLocation.indexOf(":")+1);
				// log.debug("cd /D " + drive);
				// runCommand("cd /D " + drive);
				// change dir to dumpLocation
				log.debug("mkdir -p " + dumpLocation);
				runCommand("mkdir -p " + dumpLocation);
				log.debug("cd /D " + dumpLocation);
				runCommand("cd /D " + dumpLocation);
				// generate dump in dumpLocation
				log.debug("dump the core to " + dumpLocation);
				log.debug("Command: " + command);
				runCommand(command);
			} else {
				String[] commands = { "mkdir -p " + dumpLocation,
						"cd " + dumpLocation, command };
				runShellCommand(commands);
			}

		} catch (Exception e) {
			log.error("Not able to execute command! ");
			log.error(e.getMessage());
			e.printStackTrace();
		}

		File f = new File(dumpLocation);
		if (f.exists()) {
			String[] files = f.list();
			for (int i = 0; i < files.length; i++) {
				if (files[i].contains(".dmp")) {
					generatedCoreFile = dumpLocation + Constants.FS + files[i];
					return generatedCoreFile;
				}
			}
		} else {
			log.error(dumpLocation + " doesn't exist!");
		}
		return null;
	}

	public void runCommand(String command) throws IOException {
		Runtime runtime = Runtime.getRuntime();
		Process process = null;

		try {
			process = runtime.exec(command);
			process.waitFor();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		BufferedReader stdInput = new BufferedReader(new InputStreamReader(
				process.getInputStream()));
		BufferedReader stdError = new BufferedReader(new InputStreamReader(
				process.getErrorStream()));
		String s = null;
		while ((s = stdInput.readLine()) != null) {
			log.info(s);
		}
		while ((s = stdError.readLine()) != null) {
			log.error(s);
		}
		stdInput.close();
		stdError.close();
	}

	// command "cd " doesn't work in runCommand(String command) method, we have
	// to start an instance of the shell,
	// send its commands via the Process's OutputStream
	public void runShellCommand(String[] commands) throws IOException {
		Runtime runtime = Runtime.getRuntime();
		Process process = null;

		try {

			/*
			 * if (System.getProperty("os.name").contains("windows")||System.
			 * getProperty("os.name").contains("Windows")){ process =
			 * runtime.exec("cmd /c start run.bat"); }else{
			 */
			File workDir = new File("/bin");
			process = runtime.exec("/bin/bash", null, workDir);
			// process.waitFor();
			// }

		} catch (IOException e) {
			e.printStackTrace();
		}

		if (process != null) {
			BufferedReader stdInput = new BufferedReader(new InputStreamReader(
					process.getInputStream()));
			BufferedReader stdError = new BufferedReader(new InputStreamReader(
					process.getErrorStream()));
			PrintWriter out = new PrintWriter(new BufferedWriter(
					new OutputStreamWriter(process.getOutputStream())), true);
			for (String cmd : commands) {
				out.println(cmd);
				log.info(cmd);
			}
			out.println("exit");

			try {
				String s = null;
				while ((s = stdInput.readLine()) != null) {
					log.info(s);
				}
				while ((s = stdError.readLine()) != null) {
					log.error(s);
				}

				process.waitFor();
				out.close();
				stdInput.close();
				stdError.close();
				process.destroy();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
	}

	private void setDumpName(String javaHome) {
		String platform = null;
		if (javaHome.contains("sdk") && javaHome.contains("jre")) {
			platform = javaHome.substring(javaHome.indexOf("sdk") + 4,
					javaHome.indexOf("jre") - 1);
		} else if (javaHome.contains("sdk")) {
			platform = javaHome.substring(javaHome.indexOf("sdk") + 4);
		} else {
			log.warn("Not able to get the platform. Set the dump name to j9core");
			platform = "j9core";
		}
		platform.replace(Constants.FS, "");
		dumpName = platform + ".dmp";
		log.debug("dumpName: " + dumpName);
	}
}
