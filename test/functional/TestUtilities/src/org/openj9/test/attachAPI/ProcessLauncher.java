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
package org.openj9.test.attachAPI;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.List;

import org.openj9.test.util.StringPrintStream;
import org.testng.log4testng.Logger;

/**
 * Launch a child process using a given command line and connect to the child's
 * stdin/stdout/stderr.
 *
 */
public class ProcessLauncher {

	static Logger logger = Logger.getLogger(ProcessLauncher.class);
	protected Process proc;
	protected BufferedWriter targetInWriter;
	protected BufferedReader targetOutReader;
	protected BufferedReader targetErrReader;
	protected String errOutput = ""; //$NON-NLS-1$
	protected String outOutput = ""; //$NON-NLS-1$

	/**
	 * Launch a process
	 * @param argBuffer executable name followed by arguments
	 * @return Child process or null if error
	 */
	public Process launchProcess(List<String> argBuffer) {
		String[] args;
		Process target = null;
		args = new String[argBuffer.size()];
		argBuffer.toArray(args);

		StringBuilder debugBuffer = new StringBuilder();
		debugBuffer.append("Arguments:\n"); //$NON-NLS-1$
		for (int i = 0; i < args.length; ++i) {
			debugBuffer.append(args[i]);
			debugBuffer.append(" "); //$NON-NLS-1$
		}
		debugBuffer.append("\n"); //$NON-NLS-1$
		logger.debug(debugBuffer.toString());
		Runtime myRuntime = Runtime.getRuntime();
		try {
			target = myRuntime.exec(args);
		} catch (IOException e) {
			StringPrintStream.logStackTrace(e, logger);
			return null;
		}
		OutputStream targetIn = target.getOutputStream();
		InputStream targetOut = target.getInputStream();
		InputStream targetErr = target.getErrorStream();
		targetInWriter = new BufferedWriter(new OutputStreamWriter(targetIn));
		targetOutReader = new BufferedReader(new InputStreamReader(targetOut));
		targetErrReader = new BufferedReader(new InputStreamReader(targetErr));
		return target;
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

	public String getErrOutput() {
		return errOutput;
	}

	public String getOutOutput() {
		return outOutput;
	}

	public BufferedReader getTgtOut() {
		return targetOutReader;
	}

	public BufferedReader getTgtErr() {
		return targetErrReader;
	}

}
