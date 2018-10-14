/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.threadinterrupt;

class scenario2Test {
	public static void main(String args[]) {
		scenario2Test t = new scenario2Test();
		t.Run();
	}

	public void Run() {
		Interrupter interrupter = new Interrupter(Thread.currentThread());

		interrupter.start();

		try {
			System.out.println("main  sleeping");
			Thread.sleep(100 * 1000);
			System.out.println("main  done sleeping");
			throw new Error("sleep should have been interrupted");
		} catch (InterruptedException ie) {
			System.out.println("SUCCESS");
		}
	}

	class Interrupter extends Thread {
		Thread m_interruptee;

		Interrupter(Thread interruptee) {
			m_interruptee = interruptee;
		}

		public void run() {
			System.out.println("inter waiting for main to be sleeping");
			try {
				sleep(3000);
			} catch (Exception e) {
				System.out.println("caught " + e);
			}
			System.out.println("inter interrupting main");
			m_interruptee.interrupt();
			System.out.println("inter done interrupting main");
		}
	}

}
