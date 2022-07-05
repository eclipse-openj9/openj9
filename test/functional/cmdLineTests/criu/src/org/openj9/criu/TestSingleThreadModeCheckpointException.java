/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
package org.openj9.criu;

import org.eclipse.openj9.criu.CRIUSupport;
import org.eclipse.openj9.criu.JVMCheckpointException;
import org.eclipse.openj9.criu.RestoreException;

import java.nio.file.Path;
import java.nio.file.Paths;

public class TestSingleThreadModeCheckpointException {

	private final static Path imagePath = Paths.get("cpData");
	private static final Object lock = new Object();

	public static void main(String[] args) {
		new TestSingleThreadModeCheckpointException().testSingleThreadModeCheckpointException();
	}

	private void log(String msg) {
		System.out.println(msg + ": " + this + " name: " + Thread.currentThread().getName());
	}

	Thread newThreadOwnMonitor() {
		Thread t = new Thread(new Runnable() {
			public void run() {
				log("newThreadOwnMonitor() before synchronized on " + lock);
				synchronized (lock) {
					for (;;) {
						try {
							log("newThreadOwnMonitor() before Thread.sleep()");
							Thread.sleep(1000);
							log("newThreadOwnMonitor() after Thread.sleep()");
						} catch (InterruptedException e) {
							log("newThreadOwnMonitor() interrupted");
							break;
						}
					}
				}
				log("newThreadOwnMonitor() after synchronized on " + lock);
			}
		});
		return t;
	}

	void testSingleThreadModeCheckpointException() {
		boolean result = false;
		Thread tom = newThreadOwnMonitor();
		tom.start();

		CRIUTestUtils.deleteCheckpointDirectory(imagePath);
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criu = new CRIUSupport(imagePath);
		criu.registerPreSnapshotHook(new Runnable() {
			public void run() {
				log("PreSnapshotHook() before synchronized on " + lock);
				synchronized (lock) {
					log("PreSnapshotHook() within synchronized on " + lock);
				}
				log("PreSnapshotHook() after synchronized on " + lock);
			}
		});

		try {
			System.out.println("Pre-checkpoint");
			CRIUTestUtils.checkPointJVM(criu, imagePath, false);
		} catch (JVMCheckpointException jvmce) {
			result = true;
		}
		if (result) {
			System.out.println("TestSingleThreadModeCheckpointException: PASSED.");
		} else {
			System.out.println("TestSingleThreadModeCheckpointException: FAILED.");
		}

		tom.interrupt();
		try {
			tom.join();
		} catch (InterruptedException e) {
		}
	}
}
