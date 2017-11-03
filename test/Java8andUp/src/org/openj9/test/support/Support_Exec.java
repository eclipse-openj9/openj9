package org.openj9.test.support;

/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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


import org.openj9.test.support.resource.Support_Resources;

import org.testng.Assert;
import org.testng.log4testng.Logger;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.io.StringReader;

public class Support_Exec {

	static Logger logger = Logger.getLogger(Support_Exec.class);

	public static String execJava(String[] args, String[] classpath,
			boolean displayOutput) throws IOException, InterruptedException {
		// this function returns the output of the process as a string
		Object[] execArgs = execJava2(args, classpath, displayOutput);
		Process proc = (Process) execArgs[0];

		StringBuffer output = new StringBuffer();
		InputStream in = proc.getInputStream();
		int result;
		byte[] bytes = new byte[1024];
		while ((result = in.read(bytes)) != -1) {
			output.append(new String(bytes, 0, result));
			if (displayOutput)
				System.out.write(bytes, 0, result);
		}
		in.close();
		proc.waitFor();
		checkStderr(execArgs);
		proc.destroy();
		return output.toString();
	}

	/* [PR CMVC 93383] fail test when unexpected output on err stream */
	public static void checkStderr(Object[] execArgs) {
		Process proc = (Process) execArgs[0];
		StringBuffer errBuf = (StringBuffer) execArgs[1];
		synchronized (errBuf) {
			if (errBuf.length() > 0) {
				/*
				 * [PR JAZZ 9611] Test using Runtime.exec() fails on zOS because
				 * of warning on stderr
				 */
				BufferedReader reader = new BufferedReader(new StringReader(
						errBuf.toString()));
				String line;
				try {
					while ((line = reader.readLine()) != null) {
						if (line.indexOf("switch to IFA processor") != -1) {
							continue;
						}
						Assert.fail(errBuf.toString());
					}
				} catch (IOException e) {
					Assert.fail(e.toString());
				}

			}
		}
	}

	private static String[] getExecutableFromJavaHome(String classPathString,
			boolean onUnix) {
		String execArgs[] = new String[3];
		String executable = System.getProperty("java.home");
		if (!executable.endsWith(File.separator))
			executable += File.separator;
		executable += "bin" + File.separator;
		execArgs[0] = executable + "java";
		execArgs[1] = "-cp";
		if (onUnix) {
			execArgs[2] = System.getProperty("java.class.path")
					+ classPathString;
		} else {
			execArgs[2] = "\"" + System.getProperty("java.class.path")
					+ classPathString + "\"";
		}
		return execArgs;
	}

	public static Object[] execJava2(String[] args, String[] classpath)
			throws IOException, InterruptedException {
		/* [PR 122154] -Djava.security.properties option is not supported */
		return execJava2(null, args, classpath, true);
	}

	public static Object[] execJava2(File copiedVMDir, String[] args,
			String[] classpath) throws IOException, InterruptedException {
		/* [PR 122154] -Djava.security.properties option is not supported */
		return execJava2(copiedVMDir, args, classpath, true);
	}

	public static Object[] execJava2(String[] args, String[] classpath,
			boolean displayOutput) throws IOException, InterruptedException {
		return execJava2(null, args, classpath, displayOutput);
	}

	public static Object[] execJava2(File copiedVMDir, String[] args,
			String[] classpath, boolean displayOutput) throws IOException,
			InterruptedException {
		/* [PR 122154] -Djava.security.properties option is not supported */
		// this function returns the resulting process from the exec
		String[] execArgs = getJavaCommand(copiedVMDir, args, classpath);
		return new Support_Exec().doExec(execArgs);
	}

	/* [PR 122154] -Djava.security.properties option is not supported */
	private static String[] getJavaCommand(File copiedVMDir, String[] args,
			String[] classpath) {
		int baseArgs = 0;
		String[] execArgs = null;
		String vendor = System.getProperty("java.vendor");
		boolean onUnix = File.separatorChar == '/';
		String classPathString = "";
		if (classpath != null)
			for (int i = 0; i < classpath.length; i++)
				classPathString += File.pathSeparator + classpath[i];
		if (vendor.indexOf("Sun") != -1) {
			String baseArgsArray[] = getExecutableFromJavaHome(classPathString,
					onUnix);
			baseArgs = baseArgsArray.length;
			execArgs = new String[baseArgsArray.length + args.length];
			System.arraycopy(baseArgsArray, 0, execArgs, 0,
					baseArgsArray.length);
		} else if (vendor.indexOf("IBM") != -1 || vendor.indexOf("OpenJ9") != -1) {
			String full = System.getProperty("java.fullversion");
			logger.info("***" + full + "***");
			boolean jitDisabled = false;
			if (full != null && full.indexOf("(JIT disabled") >= 0) {
				jitDisabled = true;
			}
			String rtVersion = System.getProperty("javax.realtime.version");
			boolean isRealtime = false;
			if (rtVersion != null)
				isRealtime = true;
			String jvmRT = System.getProperty("com.ibm.jvm.realtime");
			boolean isSoftRealtime = false;
			if (jvmRT != null && jvmRT.equals("soft"))
				isSoftRealtime = true;

			String vmPath = System.getProperty("com.ibm.oti.vm.exe");
			/* [PR 122154] -Djava.security.properties option is not supported */
			if (copiedVMDir != null) {
				String vmExe = new File(vmPath).getName();
				File newVm = new File(copiedVMDir, "bin");
				newVm = new File(newVm, vmExe);
				vmPath = newVm.getAbsolutePath();
			}
			if (vmPath != null) {
				baseArgs = 4;
				if (jitDisabled)
					baseArgs++;
				if (isRealtime)
					baseArgs++;
				if	(isSoftRealtime)
					baseArgs++;
				execArgs = new String[baseArgs + args.length];
				execArgs[0] = vmPath;
				String bootpath = System.getProperty("com.ibm.oti.system.class.path");
				if(bootpath == null)
					bootpath = System.getProperty("sun.boot.class.path");
				if (onUnix) {
					execArgs[1] = "-Xbootclasspath:"
							+ bootpath;
					execArgs[2] = "-cp";
					execArgs[3] = System.getProperty("java.class.path")
							+ classPathString;
				} else {
					execArgs[1] = "\"-Xbootclasspath:"
							+ bootpath
							+ "\"";
					execArgs[2] = "-cp";
					execArgs[3] = "\"" + System.getProperty("java.class.path")
							+ classPathString + "\"";
				}
				if (jitDisabled)
					execArgs[4] = "-Xint";
				if (isRealtime)
					execArgs[5] = "-Xrealtime";
				if(isSoftRealtime)
					execArgs[6] = "-Xgcpolicy:metronome";
			} else {
				String baseArgsArray[] = getExecutableFromJavaHome(
						classPathString, onUnix);
				baseArgs = baseArgsArray.length;
				if (jitDisabled)
					baseArgs++;
				if (isRealtime)
					baseArgs++;
				if	(isSoftRealtime)
					baseArgs++;
				execArgs = new String[baseArgs + args.length];
				System.arraycopy(baseArgsArray, 0, execArgs, 0,
						baseArgsArray.length);
				int baseOffset = 0;
				if (jitDisabled) {
					execArgs[baseArgsArray.length] = "-Xint";
					baseOffset++;
				}
				if (isRealtime) {
					execArgs[baseArgsArray.length + baseOffset] = "-Xrealtime";
					baseOffset++;
				}
				if (isSoftRealtime) {
					execArgs[baseArgsArray.length + baseOffset] = "-Xgcpolicy:metronome";
					baseOffset++;
				}
			}
		}

		for (int i = 0; i < args.length; i++)
			execArgs[baseArgs + i] = args[i];
		return execArgs;
	}

	public Object[] doExec(String[] execArgs) throws IOException,
			InterruptedException {
		StringBuffer command = new StringBuffer(execArgs[0]);
		for (int i = 1; i < execArgs.length; i++)
			command.append(" " + execArgs[i]);

//		log(LOG_INFO, "Exec: " + command.toString());

		final Process proc = Runtime.getRuntime().exec(execArgs);
		final StringBuffer errBuf = new StringBuffer();
		Thread errThread = new Thread(new Runnable() {
			public void run() {
				synchronized (errBuf) {
					synchronized (proc) {
						proc.notifyAll();
					}
					InputStream err = proc.getErrorStream();
					int result;
					byte[] bytes = new byte[1024];
					try {
						while ((result = err.read(bytes)) != -1) {
							System.err.write(bytes, 0, result);
							errBuf.append(new String(bytes, 0, result));
						}
						err.close();
					} catch (IOException e) {
						e.printStackTrace();
						ByteArrayOutputStream out = new ByteArrayOutputStream();
						PrintStream printer = new PrintStream(out);
						e.printStackTrace(printer);
						printer.close();
						errBuf.append(new String(out.toByteArray()));
					}
				}
			}
		});
		synchronized (proc) {
			errThread.start();
			// wait for errThread to start
			int count = 0;
			boolean done = false;
			while (!done) {
				try {
					proc.wait();
					done = true;
				} catch (InterruptedException e) {
					if (++count == 2)
						throw e;
				}
			}
			/* [PR 124106] interrupt() causes InternalError */
			// propogate the interrupt
			if (count > 0)
				Thread.currentThread().interrupt();
		}
		return new Object[] { proc, errBuf };
	}

	/* [PR 122154] -Djava.security.properties option is not supported */
	private static void xCopy(String xCopyString) {
		try {
			Process proc = Runtime.getRuntime().exec(xCopyString); //$NON-NLS-1$
			InputStream is = proc.getInputStream();
			StringBuffer msg = new StringBuffer(""); //$NON-NLS-1$
			while (true) {
				int c = is.read();
				if (c == -1)
					break;
				msg.append((char) c);
			}

			is.close();
			proc.waitFor();
			proc.destroy();
			logger.debug(msg.toString());
		} catch (IOException e) {
			// TODO Auto-generated catch block
			logger.error("Unexpected exception: " + e);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			logger.error("Unexpected exception: " + e);
		}
	}

	/* [PR 122154] -Djava.security.properties option is not supported */
	public static void deleteVM(File vm) {
		if (vm.isDirectory()) {
			File[] files = vm.listFiles();
			for (int i = 0; i < files.length; i++) {
				deleteVM(files[i]);
			}
		} else {
			vm.delete();
		}
	}

	/* [PR 122154] -Djava.security.properties option is not supported */
	public static File copyVM() {
		File tempDir = Support_Resources.createTempFolder();
		File tempVMDir = new File(tempDir, "bin"); //$NON-NLS-1$
		File vmDir = new File(System.getProperty("com.ibm.oti.vm.exe")).getParentFile(); //$NON-NLS-1$
		File javaHome = new File(System.getProperty("java.home")); //$NON-NLS-1$
		File securityDir = new File(javaHome, "lib"); //$NON-NLS-1$
		securityDir = new File(securityDir, "security"); //$NON-NLS-1$
		File tempSecurityDir = new File(tempDir, "lib"); //$NON-NLS-1$
		tempSecurityDir = new File(tempSecurityDir, "security"); //$NON-NLS-1$

		String xCopyString = "xcopy " + vmDir.getAbsolutePath() + " " + tempVMDir.getAbsolutePath() + " /I/Y"; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
		xCopy(xCopyString);

		xCopyString = "xcopy " + securityDir.getAbsolutePath() + " " + tempSecurityDir.getAbsolutePath() + " /I/Y"; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
		xCopy(xCopyString);
		return tempDir;
	}
}
