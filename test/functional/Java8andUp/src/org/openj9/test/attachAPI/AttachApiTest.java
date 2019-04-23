/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

import static org.testng.AssertJUnit.assertTrue;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.List;
import java.util.Properties;

import org.openj9.test.util.PlatformInfo;
import org.openj9.test.util.StringPrintStream;
import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.log4testng.Logger;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

@SuppressWarnings("nls")
abstract class AttachApiTest {

	protected static Logger logger = Logger.getLogger(AttachApiTest.class);
	protected String testName;
	private ArrayList<String> vmArgs;
	/* command for runCommand() */
	protected String commandName;

	protected void logExceptionInfoAndFail(Exception e) {
		logStackTrace(e);
		Assert.fail(e.getClass().getName()+": "+e.getMessage());
	}

	protected void logStackTrace(Exception e) {
		PrintStream buff = StringPrintStream.factory();
		e.printStackTrace(buff);
		logger.error(buff.toString());
	}

	protected static void log(String outLine) {
		logger.debug(outLine);
	}
	
	public void logProperties(Properties props) {
		PrintStream stream = StringPrintStream.factory();
		props.keySet().stream().sorted().forEach(k -> stream.println(k + "=" + props.getProperty((String)k))); //$NON-NLS-1$ 
		logger.debug(stream.toString());
	}

	void setVmOptions(String option) {
		if (null == vmArgs) {
			vmArgs = new ArrayList<String>();
		}
		vmArgs.add(option);

	}
	 void launchAndTestTargets(int copies, int attaches) {
		TargetManager tgts[] = new TargetManager[copies];
		for (int i = 0; i < copies; ++i) {
			tgts[i] = new TargetManager(TestUtil.TargetVM, null, vmArgs, null);
		}
		for (int i = 0; i < copies; ++i) {
			if (!tgts[i].syncWithTarget()) {
				if (TargetManager.TargetStatus.INIT_DUPLICATE_VMID
						.equals(tgts[i].getTargetVmStatus())) {
					/*
					 * could not initialize attach API due to crud in the
					 * advertisement directory
					 */
					tgts[i].terminateTarget();
					logger.warn("WARNING: attach API duplicate VMID");
				} else {
					Assert.fail("Target VM " + tgts[i].targetId
							+ " did not initialize attach API");
				}
			} else if (null != tgts[i]) {
				AssertJUnit.assertTrue("target " + tgts[i].targetId + " does not exist",
						tgts[i].vmIdExists());
			}
			logger.trace(((i % 10) == 9) ? "!" : "/");
			logger.trace("waiting");
		}
		for (int i = 0; i < copies; ++i) {
			if (!tgts[i].isActive()) {
				continue;
			}
			int attachesPerCopy = (attaches / copies) + 1;
			for (int j = 0; j < attachesPerCopy; ++j) {
				try {
					VirtualMachine vm = VirtualMachine.attach(tgts[i].getTargetPid());
					Properties props = vm.getAgentProperties();
					final String IDPROP = "j9vm.test.attach.testproperty";
					AssertJUnit.assertNull(IDPROP, props.getProperty(IDPROP));
					logger.trace(((i % 10) == 9) ? "!" : "+");
					vm.detach();
				} catch (AttachNotSupportedException e) {
					logger.error("Trying to attach to "
							+ tgts[i].getTargetPid());
					e.printStackTrace();
					try {
						logger.warn("retry attach");
						@SuppressWarnings("unused")
						VirtualMachine vm = VirtualMachine.attach(tgts[i].getTargetPid());
						logger.warn("retry successful");
					} catch (AttachNotSupportedException | IOException f) {
						logExceptionInfoAndFail(f);
					}
					Assert.fail("AttachNotSupportedException to "
							+ tgts[i].getTargetPid());
				} catch (IOException e) {
					logExceptionInfoAndFail(e);
				}
			}
		}
		for (int i = 0; i < copies; ++i) {
			tgts[i].terminateTarget();
		}
	}

	public  void launchAndTestTargets(int instances) {
		launchAndTestTargets(instances, 1);
	}

	TargetManager launchTarget() {
		return launchTarget(null);
	}

	/**
	 * launch a target process, wait for the attach API to initialize, and do a
	 * sanity check.
	 * 
	 * @param tgtName
	 *            target virtual machine ID
	 * @return launched target object
	 */
	TargetManager launchTarget(String tgtName) {
		TargetManager target = new TargetManager(TestConstants.TARGET_VM_CLASS,
				tgtName);
		target.syncWithTarget();
		checkTargetPid(target);
		return target;
	}

	public static void checkTargetPid(TargetManager target) {
		String targetPid = target.getTargetPid();
		Assert.assertTrue((null != targetPid) && !targetPid.equalsIgnoreCase("failed"), "target attach API failed to launch");
	}

	protected boolean checkLoadAgentOutput(String errOutput,
			String testString) {
		return errOutput.contains(testString);
	}

	/**
	 * launch a target command with no arguments and collect its output into an array of output lines.
	 * The command must exit on its own.
	 * 
	 * @return Output from command
	 */
	protected List<String> runCommand() throws IOException {
		return runCommand(Collections.emptyList());
	}

	/**
	 * launch a target command with arguments and collect its output into an array of output lines.
	 * The command must exit on its own.
	 * 
	 * @param list of parameters
	 * @return Output from command
	 */
	protected List<String> runCommand(List<String> args) throws IOException {
		ArrayList<String> cmdLine = new ArrayList<>();
		cmdLine.add(commandName);
		cmdLine.addAll(args);
		ProcessBuilder jpsProcess = new ProcessBuilder(cmdLine);
		Process proc = jpsProcess.start();
		List<String> outputLines = Collections.emptyList();
		try (BufferedReader jpsOutReader = new BufferedReader(new InputStreamReader(proc.getInputStream()))) {
			outputLines = jpsOutReader.lines().collect(Collectors.toList());
			proc.waitFor(1000, TimeUnit.MILLISECONDS);
		} catch (InterruptedException e) {
			/* ignore */
		}
		return outputLines;
	}

	/**
	 * Find an executable in the jdk directory and fail if it is missing.
	 * @param commandRoot Base name of the command, without ".exe" 
	 */
	protected void getJdkUtilityPath(final String commandRoot) {
		final String commandWithSuffix = 
				PlatformInfo.isWindows() ? commandRoot + ".exe" : commandRoot;
		String javaHomeString = System.getProperty("java.home");
		File javaHome = new File(javaHomeString);
		File javaHomeBin = new File(javaHome, "bin");
		File  commandFile = new File(javaHomeBin, commandWithSuffix);
		if (!commandFile.exists()) { /* Java 8 has 2 bin directories, and JAVA_HOME is probably pointing at jre */
			log("Did not find " + commandFile.getAbsolutePath());
			File javaHomeParentBin = new File(javaHome.getParentFile(), "bin");
			commandFile = new File(javaHomeParentBin, commandWithSuffix);
			assertTrue("Did not find " + commandFile.getAbsolutePath(), commandFile.exists());
		}
		commandName = commandFile.getAbsolutePath();
	}
	
}
