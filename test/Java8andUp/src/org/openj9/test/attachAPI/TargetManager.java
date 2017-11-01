/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package org.openj9.test.attachAPI;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import org.testng.log4testng.Logger;

import com.ibm.tools.attach.target.AttachHandler;
import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

@SuppressWarnings("nls")
class TargetManager {
	private static Logger logger = Logger.getLogger(TargetManager.class);
	public static final String PID_PREAMBLE = "pid=";
	public static final String VMID_PREAMBLE = "vmid=";
	public static final String STATUS_PREAMBLE = "status=";
	public static final String STATUS_INIT_SUCESS = "init_success";
	public static final String STATUS_DUPLICATE_VMID = "duplicate_vmid";
	public static final String STATUS_INIT_FAIL = "attach_init_fail";

	String targetId, displayName;
	private Process proc;
	private BufferedWriter targetInWriter;
	private BufferedReader targetOutReader;
	private BufferedReader targetErrReader;
	private OutputStream targetIn;
	String errOutput = "";
	String outOutput = "";
	private String targetPid;
	private String targetVmid;
	public static final String TARGETVM_START = "targetvm_start";
	public static final String TARGETVM_STOP = "targetvm_stop";
	static boolean verbose = false;
	private static boolean doLogging = false;
	private static final String DEFAULT_IPC_DIR = ".com_ibm_tools_attach";
	public TargetStatus targetVmStatus;
	private boolean active = true;

	public TargetStatus getTargetVmStatus() {
		return targetVmStatus;
	}

	public enum TargetStatus {
		INIT_SUCCESS, INIT_FAILURE, INIT_DUPLICATE_VMID
	}

	public static void setVerbose(boolean v) {
		verbose = v;
	}

	public synchronized String getTargetPid() {
		if (null == targetPid) {
			targetPid = "null";
		}
		return targetPid;
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

	public TargetManager(String cmdName, String targetId) {
		this.targetId = targetId;
		this.proc = launchTarget(cmdName, targetId, null, null, null);
	}

	/*
	 * target must print the PID on one line, other information on following
	 * line(s) (if any), the initialization status on the final line.
	 */
	public TargetStatus readTargetPidAndStatus() {
		TargetStatus tgtStatus = TargetStatus.INIT_FAILURE;
		StringBuffer targetLog = new StringBuffer();
		try {
			boolean done = false;
			do {
				String tgtOutput = targetOutReader.readLine();
				if (null != tgtOutput) {
					targetLog.append(tgtOutput);
					targetLog.append('\n');
				} else {
					logger.debug("TargetVM stdout closed unexpectedly");
					while (targetErrReader.ready()) {
						String currentLine = targetErrReader.readLine();
						if (null != currentLine) {
							logger.error(currentLine + "\n");
						}
					}
					tgtStatus = TargetStatus.INIT_FAILURE;
					break;
				}
				if (verbose) {
					logger.debug("TargetVM output: " + tgtOutput);
				}
				if (tgtOutput.startsWith(PID_PREAMBLE)) {
					targetPid = tgtOutput.substring(PID_PREAMBLE.length());
				}
				if (tgtOutput.startsWith(VMID_PREAMBLE)) {
					targetVmid = tgtOutput.substring(VMID_PREAMBLE.length());
				}
				if (tgtOutput.startsWith(STATUS_PREAMBLE)) {
					String statusString = tgtOutput.substring(STATUS_PREAMBLE
							.length());
					if (statusString.equals(STATUS_INIT_FAIL)) {
						if (checkForDuplicateVmid(targetPid)) {
							tgtStatus = TargetStatus.INIT_DUPLICATE_VMID;
						} else {
							tgtStatus = TargetStatus.INIT_FAILURE;
						}
					} else if (statusString.equals(STATUS_INIT_SUCESS)) {
						tgtStatus = TargetStatus.INIT_SUCCESS;
					}
					done = true;
				}
			} while (!done);
			if (null == targetPid) {
				logger.debug("Target PID not set");
				tgtStatus = TargetStatus.INIT_FAILURE;
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
		if (TargetStatus.INIT_SUCCESS != tgtStatus) {
			logger.debug("TargetVM initialization failed with status "+tgtStatus.toString()+"\nTarget output:\n"+targetLog.toString());			
		}
		return tgtStatus;
	}

	public TargetManager(String cmdName) {
		this.proc = launchTarget(cmdName, null, null, null, null);
	}

	public TargetManager(String cmdName, String targetId,
			ArrayList<String> appArgs) {
		this.targetId = targetId;
		this.proc = launchTarget(cmdName, targetId, null, null, appArgs);
	}

	public TargetManager(String cmdName, String targetId,
			ArrayList<String> vmArgs, ArrayList<String> appArgs) {
		this.targetId = targetId;
		this.proc = launchTarget(cmdName, targetId, null, vmArgs, appArgs);
	}

	public TargetManager(String cmdName, String targetId, String displayName,
			ArrayList<String> vmArgs, ArrayList<String> appArgs) {
		this.targetId = targetId;
		this.proc = launchTarget(cmdName, targetId, displayName, vmArgs,
				appArgs);
	}

	/**
	 * launch a TargetVM process
	 * 
	 * @return launched process
	 */
	private Process launchTarget(String cmdName, String tgtId,
			String myDisplayName, ArrayList<String> vmArgs,
			ArrayList<String> appArgs) {
		ArrayList<String> argBuffer = new ArrayList<String>();
		String[] args = {};
		this.displayName = myDisplayName;
		Runtime me = Runtime.getRuntime();
		char fs = File.separatorChar;
		String javaExec = System.getProperty("java.home") + fs + "bin" + fs
				+ "java";
		logger.debug("javaExec="+javaExec);
		String sideCar = System.getProperty("java.sidecar");
		String myClasspath = System.getProperty("java.class.path");
		argBuffer.add(javaExec);
		argBuffer.add("-Dcom.ibm.tools.attach.enable=yes");
		if (doLogging) {
			argBuffer.add("-Dcom.ibm.tools.attach.logging=yes");
		}
		if (null != tgtId) {
			argBuffer.add("-Dcom.ibm.tools.attach.id=" + tgtId);
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
		if (null != vmArgs) {
			argBuffer.addAll(vmArgs);
		}
		argBuffer.add(cmdName);
		if (null != appArgs) {
			argBuffer.addAll(appArgs);
		}
		Process target = null;
		args = new String[argBuffer.size()];
		argBuffer.toArray(args);
		if (verbose) {
			System.out.print("\n");
			for (int i = 0; i < args.length; ++i) {
				System.out.print(args[i] + " ");
			}
			System.out.print("\n");
		}
		try {
			target = me.exec(args);
		} catch (IOException e) {
			e.printStackTrace();
			return null;
		}
		this.targetIn = target.getOutputStream();
		InputStream targetOut = target.getInputStream();
		InputStream targetErr = target.getErrorStream();
		targetInWriter = new BufferedWriter(new OutputStreamWriter(targetIn));
		targetOutReader = new BufferedReader(new InputStreamReader(targetOut));
		targetErrReader = new BufferedReader(new InputStreamReader(targetErr));
		try {
			targetInWriter.write(TargetManager.TARGETVM_START + '\n');
			targetInWriter.flush();
		} catch (IOException e) {
			e.printStackTrace();
			return null;
		}
		return target;
	}

	boolean syncWithTarget() {
		targetVmStatus = readTargetPidAndStatus();
		if (!TargetStatus.INIT_SUCCESS.equals(targetVmStatus)) {
			return false;
		}
		if (null == targetId) {
			targetId = targetVmid;
		}
		return true;
	}

	static void listIpcDir() {
		String tmpdir = System.getProperty("java.io.tmpdir")
				+ File.separatorChar + DEFAULT_IPC_DIR;
		logger.debug("DEBUG:" + tmpdir);
		File ipcDir = new File(tmpdir);
		if (ipcDir.exists()) {
			listRecursive(System.getProperty("java.io.tmpdir"), ipcDir);
		}
	}

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

	public static boolean checkForDuplicateVmid(String targetPid) {
		String tmpdir = System.getProperty("java.io.tmpdir")
				+ File.separatorChar + DEFAULT_IPC_DIR;
		File ipcDir = new File(tmpdir, targetPid);
		return ipcDir.exists();
	}

	public boolean vmIdExists() {
		List<AttachProvider> providers = AttachProvider.providers();
		AttachProvider ap = providers.get(0);
		if (null == ap) {
			logger.error("no attach providers available");
			return false;
		}
		List<VirtualMachineDescriptor> vmds = ap.listVirtualMachines();
		Iterator<VirtualMachineDescriptor> vmi = vmds.iterator();
		while (vmi.hasNext()) {
			VirtualMachineDescriptor vm = vmi.next();
			if (targetId.equals(vm.id())
					&& ((null == displayName) || displayName.equals(vm
							.displayName()))) {
				return true;
			}
		}
		return false;
	}

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
			e.printStackTrace();
			rc = -1;
		} finally {
			try {
				/* get anything that may have been in the buffers */
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
			} catch (IOException ioe) {
				/* ignore */
			}
			
		}
		return rc;
	}

	public int terminateTarget(boolean waitForTermination) {
		int status = terminateTarget();
		if (waitForTermination) {
			try {
				status = proc.waitFor();
				logger.debug("waitFor status = " + status);
				return status;
			} catch (InterruptedException e) {
				return 1;
			}
		} else {
			return status;
		}

	}

	public synchronized String getErrOutput() {
		return errOutput;
	}

	public synchronized String getOutOutput() {
		return outOutput;
	}

	public synchronized BufferedReader getTgtOut() {
		return targetOutReader;
	}

	public synchronized BufferedReader getTgtErr() {
		return targetErrReader;
	}

	public static void setLogging(boolean enableLogging) {
		doLogging = enableLogging;
	}

	/*
	 * find log files, print them, then delete them
	 */
	public static void dumpLogs() {
		dumpLogs(true);
	}

	/*
	 * find log files, optionally print them, then delete them
	 * 
	 * @param printLogs set to true if the files are to be printed before
	 * deleting
	 */
	public static void dumpLogs(boolean printLogs) {
		logger.debug("Dumping attach API logs");
		File pwd = new File(System.getProperty("user.dir"));
		File[] logFiles = pwd.listFiles();
		String myLog = AttachHandler.getVmId() + ".log";
		for (File f : logFiles) {
			String logName = f.getName();
			if (logName.equalsIgnoreCase("smit.log")) {
				continue; /* AIX system log */
			}
			if (logName.endsWith(".log")) {
				if (printLogs) {
					try {
						logger.debug("Log file " + f.getName()+":\n" 
								+ (new String(Files.readAllBytes(Paths.get(f.toURI())))));
					} catch (FileNotFoundException e) {
						e.printStackTrace();
					} catch (IOException e) {
						e.printStackTrace();
					}
				}
				if (!logName.equalsIgnoreCase(myLog)) {
					if (!f.delete()) {
						logger.error("could not delete " + f.getName());
					}
				}
			}
		}
	}

	public boolean isActive() {
		return active;
	}

}
