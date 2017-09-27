/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *******************************************************************************/
package org.openj9.test.java9AttachAPI;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.List;

import org.testng.log4testng.Logger;

/* This is was copied from org.openj9.test.attachAPI.TargetManager in Java8AndUp.
 * Unnecessary code has been trimmed.
 */
@SuppressWarnings("nls")
class TargetManager {
	public static final String VMID_PREAMBLE = "vmid=";
	public static final String TARGETVM_STOP = "targetvm_stop";
	public static final String TARGETVM_START = "targetvm_start";
	public static final String STATUS_PREAMBLE = "status=";
	public static final String STATUS_INIT_SUCESS = "init_success";
	public static final String STATUS_INIT_FAIL = "attach_init_fail";
	private static final String DEFAULT_IPC_DIR = ".com_ibm_tools_attach";

	static boolean verbose = false;
	private static boolean doLogging = false;
	private static Logger logger = Logger.getLogger(TargetManager.class);

	private BufferedReader targetErrReader;
	private BufferedReader targetOutReader;
	private BufferedWriter targetInWriter;
	private OutputStream targetIn;
	private Process proc;
	private String displayName;
	private String errOutput = "";
	private String outOutput = "";
	private String targetId;
	private String targetVmid;
	private boolean active = true;
	public TargetStatus targetVmStatus;

	public TargetStatus getTargetVmStatus() {
		return targetVmStatus;
	}

	public enum TargetStatus {
		INIT_SUCCESS, INIT_FAILURE
	}

	public static void setVerbose(boolean v) {
		verbose = v;
	}

	/**
	 * @param cmdName Classname of the Java program to launch
	 * @param targetId optional attach API target ID
	 */
	public TargetManager(String cmdName, String targetId) {
		this.targetId = targetId;
		this.proc = launchTarget(cmdName, targetId, null, null, null);
	}

	/**
	 * @param cmdName Classname of the Java program to launch
	 */
	public TargetManager(String cmdName) {
		this.proc = launchTarget(cmdName, null, null, null, null);
	}

	/**
	 * @param cmdName Classname of the Java program to launch
	 * @param targetId optional attach API target ID
	 * @param appArgs Application arguments
	 */
	public TargetManager(String cmdName, String targetId,
			List<String> appArgs) {
		this.targetId = targetId;
		this.proc = launchTarget(cmdName, targetId, null, null, appArgs);
	}

	/**
	 * @param cmdName Classname of the Java program to launch
	 * @param targetId optional attach API target ID
	 * @param vmArgs VM arguments
	 * @param appArgs Application arguments
	 */
	public TargetManager(String cmdName, String targetId,
			List<String> vmArgs, List<String> appArgs) {
		this.targetId = targetId;
		this.proc = launchTarget(cmdName, targetId, null, vmArgs, appArgs);
	}

	/**
	 * @param cmdName Classname of the Java program to launch
	 * @param targetId optional attach API target ID
	 * @param displayName
	 * @param vmArgs VM arguments
	 * @param appArgs Application arguments
	 */
	public TargetManager(String cmdName, String targetId, String displayName,
			List<String> vmArgs, List<String> appArgs) {
		this.targetId = targetId;
		this.proc = launchTarget(cmdName, targetId, displayName, vmArgs,
				appArgs);
	}

	/**
	 * launch a TargetVM process.  Copy the relevant environment such as VM options and 
	 * classpath to the target's command line.
	 * @param cmdName Classname of the Java program to launch
	 * @param targetId optional attach API target ID
	 * @param myDisplayName  optional attach API target display name
	 * @param vmArgs VM arguments, such as system property settings
	 * @param appArgs Application arguments
	 * @return launched process
	 */
	private Process launchTarget(String cmdName, String targetId,
			String myDisplayName, List<String> vmArgs,
			List<String> appArgs) {
		
		/* Used to build up a list of command line arguments */
		List<String> argBuffer = new ArrayList<String>();
		String[] args = {};
		this.displayName = myDisplayName;
		Runtime me = Runtime.getRuntime();
		char fs = File.separatorChar;
		
		/* grab the location of the java launcher. */
		String javaExec = System.getProperty("java.home") + fs + "bin" + fs
				+ "java";
		logger.debug("javaExec="+javaExec);
		
		/* These are the VM options to the parent process, which we wish to pass on to the child. */
		String sideCar = System.getProperty("java.sidecar"); 
		String myClasspath = System.getProperty("java.class.path");
		argBuffer.add(javaExec);
		argBuffer.add("-Dcom.ibm.tools.attach.enable=yes");
		if (doLogging) {
			argBuffer.add("-Dcom.ibm.tools.attach.logging=yes");
		}
		if (null != targetId) {
			argBuffer.add("-Dcom.ibm.tools.attach.id=" + targetId);
		}
		if (null != myDisplayName) {
			argBuffer.add("-Dcom.ibm.tools.attach.displayName=" + myDisplayName);
		}
		argBuffer.add("-classpath");
		argBuffer.add(myClasspath);
		if ((null != sideCar) && (sideCar.length() > 0)) {
			String sidecarArgs[] = sideCar.split(" +");
			for (String s : sidecarArgs) {
				argBuffer.add(s);
			}
		}
		/* Add any extra VM arguments */
		if (null != vmArgs) {
			argBuffer.addAll(vmArgs);
		}
		
		argBuffer.add(cmdName);
		
		/* Add any application arguments */
		if (null != appArgs) {
			argBuffer.addAll(appArgs);
		}
		Process target = null;
		args = new String[argBuffer.size()];
		
		/* Build the complete command line. */
		argBuffer.toArray(args);
		if (verbose) {
			System.out.print("\n");
			for (int i = 0; i < args.length; ++i) {
				System.out.print(args[i] + " ");
			}
			System.out.print("\n");
		}
		
		/* Start the target process. */
		try {
			target = me.exec(args);
		} catch (IOException e) {
			logger.error("launchTarget failed", e);
			return null;
		}
		
		/* Grab the input/output streams. */
		this.targetIn = target.getOutputStream();
		InputStream targetOut = target.getInputStream();
		InputStream targetErr = target.getErrorStream();
		targetInWriter = new BufferedWriter(new OutputStreamWriter(targetIn));
		targetOutReader = new BufferedReader(new InputStreamReader(targetOut));
		targetErrReader = new BufferedReader(new InputStreamReader(targetErr));
		
		/* 
		 * Typical usage is to control the target process via text commands on stdin.
		 * Tell the target process to do its thing. 
		 */
		try {
			targetInWriter.write(TargetManager.TARGETVM_START + '\n');
			targetInWriter.flush();
		} catch (IOException e) {
			logger.error("launchTarget failed", e);
			return null;
		}
		return target;
	}

	/**
	 * target must print the PID on one line, other information on following
	 * line(s) (if any), the initialization status on the final line.
	 * @return TargetStatus: INIT_SUCCESS or INIT_FAILURE
	 */
	public TargetStatus readTargetStatus() {
		TargetStatus targetStatus = TargetStatus.INIT_FAILURE;
		StringBuffer targetLog = new StringBuffer();
		try {
			boolean done = false;
			do {
				String targetOutput = targetOutReader.readLine();
				if (null != targetOutput) {
					targetLog.append(targetOutput);
					targetLog.append('\n');
				} else {
					logger.debug("TargetVM stdout closed unexpectedly");
					/* stderr may not have closed, so grab as much of it as we can without blocking. */
					while (targetErrReader.ready()) {
						String currentLine = targetErrReader.readLine();
						if (null != currentLine) {
							logger.error(currentLine + "\n");
						}
					}
					targetStatus = TargetStatus.INIT_FAILURE;
					break;
				}
				if (verbose) {
					logger.debug("TargetVM output: " + targetOutput);
				}
				if (targetOutput.startsWith(VMID_PREAMBLE)) {
					setTargetVmid(targetOutput.substring(VMID_PREAMBLE.length()));
				}
				if (targetOutput.startsWith(STATUS_PREAMBLE)) {
					String statusString = targetOutput.substring(STATUS_PREAMBLE
							.length());
					if (statusString.equals(STATUS_INIT_FAIL)) {
						targetStatus = TargetStatus.INIT_FAILURE;
					} else if (statusString.equals(STATUS_INIT_SUCESS)) {
						targetStatus = TargetStatus.INIT_SUCCESS;
					}
					done = true;
				}
			} while (!done);
		} catch (IOException e) {
			logger.error("readTargetStatus failed", e);
		}
		if (TargetStatus.INIT_SUCCESS != targetStatus) {
			logger.debug("TargetVM initialization failed with status "+targetStatus.toString()+"\nTarget output:\n"+targetLog.toString());			
		}
		return targetStatus;
	}

	/**
	 * Instruct the child process to exit and grab its outputs.
	 * @return exit status of process.
	 */
	protected int terminateTarget() {
		int rc = -1;
		if (!active) {
			return 0;
		}
		active = false;
		try {
			targetInWriter.write(TargetManager.TARGETVM_STOP + '\n');
			try {
				targetInWriter.flush();
			} catch (IOException e) {
				/* ignore it */
			}
			while (targetErrReader.ready()) {
				String currentLine = targetErrReader.readLine();
				if ((null != currentLine) &&
						!currentLine.startsWith("JVMJ9VM082E Unable to switch to IFA processor")) {
					errOutput += currentLine + "\n";
				}
			}
			while (targetOutReader.ready()) {
				String currentLine = targetOutReader.readLine();
				if (null != currentLine) {
					outOutput += currentLine + "\n";
				}
			}
			rc = proc.waitFor();
			proc.destroy();
		} catch (IOException e) { 
			/* target closed the streams */
		} catch (InterruptedException e) {
			logger.error("terminateTarget failed", e);
			rc = -1;
		} finally {
			try {
				/* 
				 * Get anything that may have been in the buffers. 
				 * The streams may not have been closed, so read without blocking.
				 */
				while (targetErrReader.ready()) {
					String currentLine = targetErrReader.readLine();
					if (null != currentLine) {
						errOutput += currentLine + "\n";
					}
				}
				while (targetOutReader.ready()) {
					String currentLine = targetOutReader.readLine();
					if (null != currentLine) {
						outOutput += currentLine + "\n";
					}
				}
				logger.info("id="+targetId+" display name="+displayName+" exit status ="+rc);
				logger.info("stdout:\n"+outOutput);
				logger.info("stderr:\n"+errOutput);
			} catch (IOException ioe) {
				/* ignore */
			}
	
		}
		return rc;
	}

	/**
	 * Wait for the child process to terminate and return its exit status
	 * @return exit status
	 * @throws InterruptedException
	 */
	int waitFor() throws InterruptedException {
		int exitStatus = proc.waitFor();
		return exitStatus;
	}

	/**
	 * List the contents of the directory holding OpenJ9 attach API artifacts.
	 */
	static void listIpcDir() {
		String tmpdir = System.getProperty("java.io.tmpdir")
				+ File.separatorChar + DEFAULT_IPC_DIR;
		logger.debug("DEBUG:" + tmpdir);
		File ipcDir = new File(tmpdir);
		if (ipcDir.exists()) {
			listRecursive(System.getProperty("java.io.tmpdir"), ipcDir);
		}
	}

	/**
	 * List the contents of  directory, including subdirectories.
	 * @param prefix
	 * @param root
	 */
	static void listRecursive(String prefix, File root) {
		String myPath = prefix + "/" + root.getName();
		logger.debug(myPath);
		if (root.isDirectory()) {
			File[] children = root.listFiles();
			if (null != children) {
				for (File c : children) {
					listRecursive(myPath, c);
				}
			}
		}
	}

	public static void setLogging(boolean enableLogging) {
		doLogging = enableLogging;
	}

	public boolean isActive() {
		return active;
	}

	private void setTargetVmid(String id) {
		targetVmid =id;
	}

	public String getTargetVmid() {
		return targetVmid;
	}

	public BufferedWriter getTargetInWriter() {
		return targetInWriter;
	}

	public BufferedReader getTargetOutReader() {
		return targetOutReader;
	}

	public BufferedReader getTargetErrReader() {
		return targetErrReader;
	}
}
