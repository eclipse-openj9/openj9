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

public class TestSingleThreadModeRestoreException {

	private static final Object lock = new Object();

	public static void main(String[] args) {
		new TestSingleThreadModeRestoreException().testSingleThreadModeRestoreException();
	}

	Thread doCheckpoint() {
		Thread t = new Thread(new Runnable() {
			public void run() {
				Path imagePath = Paths.get("cpData");
				CRIUTestUtils.deleteCheckpointDirectory(imagePath);
				CRIUTestUtils.deleteCheckpointDirectory(imagePath);
				CRIUTestUtils.createCheckpointDirectory(imagePath);
				CRIUSupport criu = new CRIUSupport(imagePath);
				criu.registerPostRestoreHook(new Runnable() {
					public void run() {
						CRIUTestUtils.showThreadCurrentTime("PreCheckpointHook() before synchronized on " + lock);
						synchronized (lock) {
							CRIUTestUtils.showThreadCurrentTime("PreCheckpointHook() within synchronized on " + lock);
						}
						CRIUTestUtils.showThreadCurrentTime("PreCheckpointHook() after synchronized on " + lock);
					}
				});

				System.out.println("Pre-checkpoint");
				CRIUTestUtils.checkPointJVM(criu, imagePath, false);
			}
		});
		return t;
	}

	void testSingleThreadModeRestoreException() {
		CRIUTestUtils.showThreadCurrentTime("testSingleThreadModeRestoreException() before synchronized on " + lock);
		synchronized (lock) {
			try {
				// ensure the lock already taken before performing a checkpoint
				CRIUTestUtils.showThreadCurrentTime("testSingleThreadModeRestoreException() before doCheckpoint()");
				Thread tpc = doCheckpoint();
				tpc.start();
				// set timeout 10s
				tpc.join(10000);
				CRIUTestUtils.showThreadCurrentTime("testSingleThreadModeRestoreException() after doCheckpoint()");
			} catch (InterruptedException e) {
			}
		}
		CRIUTestUtils.showThreadCurrentTime("testSingleThreadModeRestoreException() after synchronized on " + lock);
	}
}
