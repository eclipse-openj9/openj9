/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

package org.openj9.test.java.lang.management;
import java.io.File;
import java.io.UnsupportedEncodingException;

public class RemoteProcess {

	/**
	 * Start the RemoteProcess with jmx enabled, for example:
	 * -Dcom.sun.management.jmxremote.port=9999
	 * -Dcom.sun.management.jmxremote.authenticate=false
	 * -Dcom.sun.management.jmxremote.ssl=false
	 */
	public static void main(String[] args) {
		System.out.println("=========RemoteProcess Starts!=========");
		for (int i = 0; i < 60; i++) {
			try {
				int[] a = new int[1024 * 1024];
				new String("hello").getBytes("Unsupported");
			} catch (UnsupportedEncodingException e) {
				/* Expected exception from getBytes() hence ignored */
			}

			try {
				String currentDir = System.getProperty("user.dir");
				File file = new File(currentDir + File.separator + "catch.txt");
				if (!file.exists()) {
					if (!file.createNewFile()) {
						System.out.println("Creating of catch.txt failed!");
					}
				}
				Thread.sleep(1000);
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
		System.out.println("RemoteProcess Stops!");
	}
}
