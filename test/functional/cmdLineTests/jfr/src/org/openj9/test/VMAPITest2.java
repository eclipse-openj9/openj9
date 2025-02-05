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

public class VMAPITest2 {
	public static void main(String[] args) throws Throwable {
		final WorkLoad workLoad = new WorkLoad(200, 20000, 200);

		Thread app = new Thread(() -> {
			workLoad.runWork();
		});
		app.start();

		try {
			Thread.sleep(3000);
		} catch(Throwable t) {
			t.printStackTrace();
		}

		if (VM.isJFRRecordingStarted()) {
			System.out.println("Failed should not be recording 1.");
			System.exit(0);
		}

		/* attempt a stop recording before it has started */
		VM.stopJFR();

		if (VM.isJFRRecordingStarted()) {
			System.out.println("Failed should not be recording 2.");
			System.exit(0);
		}

		VM.startJFR();
		if (!VM.isJFRRecordingStarted()) {
			System.out.println("Failed should be recording 3.");
			System.exit(0);
		}


		/* attempt another start recording after one has already started */
		VM.startJFR();
		if (!VM.isJFRRecordingStarted()) {
			System.out.println("Failed should be recording 4.");
			System.exit(0);
		}

		VM.stopJFR();
		if (VM.isJFRRecordingStarted()) {
			System.out.println("Failed should not be recording 5.");
			System.exit(0);
		}

		/* attempt a stop recording before it has started */
		VM.stopJFR();
	}
}
