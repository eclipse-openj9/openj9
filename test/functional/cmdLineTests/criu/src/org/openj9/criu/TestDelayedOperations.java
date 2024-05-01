/*
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
 */
package org.openj9.criu;

import java.nio.file.Path;
import java.nio.file.Paths;
import org.eclipse.openj9.criu.CRIUSupport;
import org.eclipse.openj9.criu.JVMCheckpointException;

public class TestDelayedOperations {

	public static void main(String[] args) {
		new TestDelayedOperations().testDelayedThreadInterrupt();
	}

	private void testDelayedThreadInterrupt() {

		final Thread threadAwaiting = new Thread(() -> {
			Path imagePath = Paths.get("cpData");
			CRIUTestUtils.deleteCheckpointDirectory(imagePath);
			CRIUTestUtils.createCheckpointDirectory(imagePath);
			CRIUSupport criu = new CRIUSupport(imagePath);
			final Thread currentThread = Thread.currentThread();
			CRIUTestUtils.showThreadCurrentTime(
					"currentThread : " + currentThread + " with name : " + currentThread.getName());
			criu.registerPreCheckpointHook(new Runnable() {
				public void run() {
					CRIUTestUtils.showThreadCurrentTime(
							"PreCheckpointHook() before threadAwaiting.interrupt() - currentThread : " + currentThread);
					currentThread.interrupt();
					if (currentThread.isInterrupted()) {
						System.out.println(
								"TestDelayedOperations.testDelayedThreadInterrupt(): FAILED at PreCheckpointHook()");
					}
					CRIUTestUtils.showThreadCurrentTime("PreCheckpointHook() after threadAwaiting.interrupt()");
				}
			});
			criu.registerPostRestoreHook(new Runnable() {
				public void run() {
					CRIUTestUtils.showThreadCurrentTime(
							"PostSnapshotHook() before threadAwaiting.interrupt() - currentThread : " + currentThread);
					currentThread.interrupt();
					if (currentThread.isInterrupted()) {
						System.out.println(
								"TestDelayedOperations.testDelayedThreadInterrupt(): FAILED at PostSnapshotHook()");
					}
					CRIUTestUtils.showThreadCurrentTime("PostSnapshotHook() after threadAwaiting.interrupt()");
				}
			});

			System.out.println("Pre-checkpoint - currentThread : " + currentThread);
			CRIUTestUtils.checkPointJVM(criu, imagePath, false);
			try {
				Thread.sleep(2000);
				System.out.println("TestDelayedOperations.testDelayedThreadInterrupt(): FAILED - not interrrupted");
			} catch (InterruptedException e) {
				System.out.println("TestDelayedOperations.testDelayedThreadInterrupt(): PASSED.");
			}
		});
		threadAwaiting.start();
		try {
			threadAwaiting.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}
}
