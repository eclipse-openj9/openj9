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
package j9vm.test.corehelper;

public class TestJITExtHelperThread {
	boolean isWaiting = false;
	Thread jittedThread = null;

	public void configureJittedHelperThread() {
		jittedThread = new Thread() {
			public void run() {
				waitForDump();
			}
		};

		jittedThread.start();

		/* wait to start core dump until thread is in a waiting state */
		synchronized(this) {
			while(!isWaiting) {
				try {
					wait(0,0);
				} catch (InterruptedException e) {}
			}
		}
	}

	void waitForDump() {
		synchronized(this) {
			/* signal to main thread that it is safe to start the core dump */
			isWaiting = true;
			notifyAll();

			while (isWaiting) {
				try {
					wait(0, 0);
				} catch (InterruptedException e) {}
			}
		}
	}

	public void endJittedHelperThread() {
		synchronized(this) {
			isWaiting = false;
			notifyAll();
		}

		try {
			jittedThread.join();
		} catch(InterruptedException e) {}
	}
}
