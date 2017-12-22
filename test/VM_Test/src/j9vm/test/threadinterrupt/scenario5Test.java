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

class scenario5Test {
	public static void main(String args[]) {
		scenario5Test t = new scenario5Test();
		t.Run();
	}

	public void Run() {
		Object a = new Object();
		Interrupter interrupter = new Interrupter(Thread.currentThread(), a);

		synchronized (a) {
			interrupter.start();
			try {
				System.out.println("main  waiting");
				a.wait();
				System.out.println("main  done waiting");
			} catch (InterruptedException e) {
				/* CMVC 112391
				 * 3-tier-enabled platforms will probably throw InterruptedException.
				 * 3-tier-disabled platforms won't throw the exception.
				 */
				System.out.println("main  caught InterruptedException");
			}
		}
		System.out.println("SUCCESS");
	}

	class Interrupter extends Thread {
		Thread m_interruptee;
		Object m_a;

		Interrupter(Thread interruptee, Object a) {
			m_interruptee = interruptee;
			m_a = a;
		}

		public void run() {
			try {
				synchronized (m_a) {
					System.out.println("inter synced on a");
					System.out.println("inter interrupting main");
					m_interruptee.interrupt();
					System.out.println("inter done interrupting main");
					sleep(1000);
					System.out.println("inter notifying main");
					m_a.notify();
					System.out.println("inter done notifying main");
				}
			} catch (Exception e) {
				System.out.println("Interrupter exception " + e);
			}
		}
	}

}
