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

import org.openj9.test.management.util.BusyThread;
import org.testng.log4testng.Logger;

/**
 * Start a server that busy loops. This is used by the JMX Beans tests
 * to get usage info for a remote process
 */
public class RemoteServer {

	private static final Logger logger = Logger.getLogger(RemoteServer.class);

	static final int SLEEPINTERVAL = 5;
	static final int NTHREADS = 1;
	static final int NITERATIONS = 10;

	/**
	 * Start the RemoteServer with jmx enabled, for example:
	 * -Dcom.sun.management.jmxremote.port=9999
	 * -Dcom.sun.management.jmxremote.authenticate=false
	 * -Dcom.sun.management.jmxremote.ssl=false
	 */
	public static void main(String[] args) {
		int i;
		long counter = 0;

		/* We must write something to stdout because the client waits for
		 * this before it proceeds
		 */
		logger.info("=========RemoteServer Starts!=========");

		try {
			Thread[] busyObj = new Thread[NTHREADS];

			/* keep the server process running for 20 iterations . */
			for (; counter < NITERATIONS; counter++) {

				for (i = 0; i < NTHREADS; i++) {
					busyObj[i] = new Thread(new BusyThread(SLEEPINTERVAL * 1000));
					busyObj[i].start();
				}

				/* sleep the server once in a while so that it may respond to client. */
				Thread.sleep(1000);

				for (i = 0; i < NTHREADS; i++) {
					busyObj[i].join();
				}
			}
		} catch (InterruptedException e) {
			logger.error("Unexpected InterruptedException occured", e);
		}

		logger.info("RemoteTestServer Stops!");
	}

}
