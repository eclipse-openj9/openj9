/*
 * Copyright IBM Corp. and others 2024
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
package org.openj9.test;

import com.ibm.oti.vm.VM;

public class VMAPITest {
	public static void main(String[] args) throws Throwable {
		final WorkLoad workLoad = new WorkLoad(200, 20000, 200);

		if (VM.isJFRRecordingStarted()) {
			System.out.println("Failed should not be recording.");
			return;
		}

		Thread app = new Thread(() -> {
			workLoad.runWork();
		});
		app.start();

		for (int i = 0; i < 15; i++) {
			Thread.sleep(100);

			if (VM.startJFR() != 0) {
				System.out.println("Failed to start.");
				return;
			}

			if (!VM.isJFRRecordingStarted()) {
				System.out.println("Failed to record.");
				return;
			}

			if (!VM.setJFRRecordingFileName("sample" + i + ".jfr")) {
				System.out.println("Failed to set name.");
				return;
			}

			Thread.sleep(1000);
			VM.jfrDump();
			VM.stopJFR();

			if (VM.isJFRRecordingStarted()) {
				System.out.println("Failed to stop recording.");
				return;
			}
		}
	}
}
