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
package org.openj9.test.management;

import java.io.*;
import java.io.BufferedReader;

import org.testng.log4testng.Logger;

public class ProcessLocking {

	public static final Logger logger = JvmCpuMonitorMXBeanTest.logger;

	private String tmpdir;
	private File file;
	private InputStream in;
	private InputStreamReader inStream;
	private BufferedReader br;

	public ProcessLocking(String filename) {
		try {
			file = new File(filename);
		} catch (Exception e) {
			logger.error("Unexpected exception occured " + e.getMessage(), e);
		}
	}

	public void waitForEvent(String condition) {
		String line;
		try {
			do {
				in = new FileInputStream(file);
				inStream = new InputStreamReader(in);
				br = new BufferedReader(inStream);
				// this call will block until it is able to obtain an
				// exclusive lock on the file.
				line = br.readLine();
				br.close();
				if (null != line && line.contains(condition)) {
					// Event has occurred; remove the line and leave.
					logger.info(line);
					PrintWriter writer = new PrintWriter(file);
					writer.print("");
					writer.close();
					break;
				} else {
					// Event yet to occur; check after some time. Since the
					// event hasn't occurred yet, holding lock will prevent
					// the notifying process from acquiring lock for writing.
					Thread.sleep(1000);
				}
			} while (true);
		} catch (Exception e) {
			logger.error("Exception caught in waitForEvent " + e.getMessage(), e);
		}
	}

	public void notifyEvent(String condition) {
		String line;
		try {
			do {
				in = new FileInputStream(file);
				inStream = new InputStreamReader(in);
				br = new BufferedReader(inStream);
				line = br.readLine();
				br.close();
				if (null == line) {
					PrintWriter writer = new PrintWriter(file);
					writer.println(condition);
					writer.close();
					break;
				} else {
					Thread.sleep(1000);
				}
			} while (true);
		} catch (Exception e) {
			logger.error("Exception caught in notifyEvent " + e.getMessage(), e);
		}
	}
}
