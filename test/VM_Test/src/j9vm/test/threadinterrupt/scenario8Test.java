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

class scenario8Test {
	public static void main(String args[]) {
		scenario8Test t = new scenario8Test();
		t.Run();
	}

	void Print(String s) {
		synchronized (System.out) {
			System.out.print(System.currentTimeMillis() + ": ");
			System.out.println(s);
		}
	}

	public void Run() {
		Object a = new Object();
		Object b = new Object();
		Interrupter interrupter = new Interrupter(Thread.currentThread(), b);
		interrupter.start();
		Owner owner = new Owner(a, b);
		owner.start();

		Print("main syncing on a ");
		synchronized (a) {
			Print("main synced on a ");
			try {
				Print("main  waiting");
				a.wait();
				Print("main  done waiting");
				throw new Error("wait should have been interrupted");
			} catch (InterruptedException ie) {
				Print("SUCCESS	 - Caught exception " + ie);
			}
		}
		Print(Thread.currentThread() + " (main) is done");
	}

	class Owner extends Thread {
		Object m_a;
		Object m_b;

		Owner(Object a, Object b) {
			m_a = a;
			m_b = b;
		}

		public void run() {
			try {
				sleep(2 * 1000);
				Print("owner syncing on a");
				synchronized (m_a) {
					Print("owner synced on a");
					Print("owner syncing on b");
					synchronized (m_b) {
						Print("owner synced on b");
						Print("owner releasing b");
					}
					Print("owner released b");
					Print("owner releasing a");
				}
				Print("owner released a");
			} catch (Exception e) {
				Print(this + " caught " + e);
			}
			Print(this + " (owner) is done");
		}
	}

	class Interrupter extends Thread {
		Thread m_interruptee;

		Object m_b;

		Interrupter(Thread interruptee, Object b) {
			m_interruptee = interruptee;
			m_b = b;
		}

		public void run() {
			try {
				sleep(1000);
				Print("inter syncing on b");
				synchronized (m_b) {
					Print("inter synced on b");
					sleep(6 * 1000);
					Print("inter interrupting main");
					m_interruptee.interrupt();
					Print("inter done interrupting main");
					Print("inter releasing b");
				}
				Print("inter released b");
				Print(this + " (inter) is done");
			} catch (Exception e) {
				Print("inter caught exception " + e);
			}
		}
	}
}
