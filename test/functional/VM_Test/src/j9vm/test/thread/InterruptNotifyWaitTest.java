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
package j9vm.test.thread;

/*
 * Test contention between waiting, notifying, and interrupting threads.
 */
public class InterruptNotifyWaitTest {
	Counter notifyWake = new Counter();
	Counter intrWake = new Counter();
	Counter progress = new Counter();
	
	int sec = 150;
	int notifyThreads = 10;

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		new InterruptNotifyWaitTest().run(args);
	}

	public void run(String[] args) {
		Object sync = new Object();
		WaitThread w = new WaitThread(sync);
		NotifyThread n[] = new NotifyThread[notifyThreads];
		IntrThread r = new IntrThread(w);
		IntrThread r2 = new IntrThread(w);

		if (args.length > 0) {
			sec = Integer.parseInt(args[0]);
		}

		for (int i = 0; i < n.length; i++) {
			n[i] = new NotifyThread(sync);
		}
		
		w.start();
		for (int i = 0; i < n.length; i++) {
			n[i].start();
		}
		r.start();
		r2.start();
		
		long nowakeups = 0;
		long oldprogress = 0;
		long newprogress = 0;

		try {
			for (int i = 0; i < sec; i++) {
//				System.out.print(".");
				Thread.sleep(1000);
				
				/* Check for symptoms of deadlock */
				newprogress = progress.get();
				if (oldprogress == newprogress) {
					nowakeups++;
				} else {
					nowakeups = 0;
					oldprogress = newprogress;
				}
			}
		} catch (InterruptedException e) {
		}
		System.out.println("intr: " + intrWake.get() + " notify: " + notifyWake.get());
		if (nowakeups > 1) {
			r.stop();
			r2.stop();
			for (int i = 0; i < n.length; i++) {
				n[i].stop();
			}
			w.stop();
			throw new Error("potential deadlock: no progress for " + nowakeups + "s");
		}
		System.exit(0);
	}

	class WaitThread extends Thread {
		Object sync;

		public void run() {
			while (true) {
				dowait();
				progress.add();
			}
		}

		WaitThread(Object sync) {
			this.sync = sync;
		}

		void dowait() {
			synchronized (sync) {
				try {
					sync.wait();
					notifyWake.add();
				} catch (InterruptedException e) {
					intrWake.add();
				}
			}
		}
	}

	class NotifyThread extends Thread {
		Object sync;
		
		NotifyThread(Object sync) {
			this.sync = sync;
		}

		public void run() {
			while (true) {
				synchronized (sync) {
					sync.notify();
				}
				progress.add();
			}
		}
	}

	class IntrThread extends Thread {
		Thread target;

		IntrThread(Thread target) {
			this.target = target;
		}

		public void run() {
			while (true) {
				try {
					/*
					 * Reduce frequency of interrupts so that notifies can
					 * happen
					 */
					Thread.sleep(147);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
				target.interrupt();
				progress.add();
			}
		}
	}
	
	static class Counter {
		private long counter = 0;
		synchronized void add() {
			counter++;
		}
		synchronized long get() {
			return counter;
		}
	}
}
