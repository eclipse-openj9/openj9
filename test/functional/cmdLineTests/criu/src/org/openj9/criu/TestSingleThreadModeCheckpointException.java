/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
package org.openj9.criu;

import org.eclipse.openj9.criu.CRIUSupport;
import org.eclipse.openj9.criu.JVMCheckpointException;

import java.nio.file.Path;
import java.nio.file.Paths;

public class TestSingleThreadModeCheckpointException {

	private static final Object lock = new Object();

	public static void main(String[] args) {
		new TestSingleThreadModeCheckpointException().testSingleThreadModeCheckpointException();
	}

	Thread doCheckpoint() {
		Thread t = new Thread(new Runnable() {
			public void run() {
				boolean result = false;
				Path imagePath = Paths.get("cpData");
				CRIUTestUtils.deleteCheckpointDirectory(imagePath);
				CRIUTestUtils.createCheckpointDirectory(imagePath);
				CRIUSupport criu = new CRIUSupport(imagePath);
				criu.registerPreCheckpointHook(new Runnable() {
					public void run() {
						CRIUTestUtils.showThreadCurrentTime("PreCheckpointHook() before synchronized on " + lock);
						synchronized (lock) {
							CRIUTestUtils.showThreadCurrentTime("PreCheckpointHook() within synchronized on " + lock);
						}
						CRIUTestUtils.showThreadCurrentTime("PreCheckpointHook() after synchronized on " + lock);
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
			}
		});
		return t;
	}

	void testSingleThreadModeCheckpointException() {
		CRIUTestUtils.showThreadCurrentTime("testSingleThreadModeCheckpointException() before synchronized on " + lock);
		synchronized (lock) {
			try {
				// ensure the lock already taken before performing a checkpoint
				CRIUTestUtils.showThreadCurrentTime("testSingleThreadModeCheckpointException() before doCheckpoint()");
				Thread tpc = doCheckpoint();
				tpc.start();
				// set timeout 10s
				tpc.join(10000);
				CRIUTestUtils.showThreadCurrentTime("testSingleThreadModeCheckpointException() after doCheckpoint()");
			} catch (InterruptedException e) {
			}
		}
		CRIUTestUtils.showThreadCurrentTime("testSingleThreadModeCheckpointException() after synchronized on " + lock);
	}
}
