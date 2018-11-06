/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

public class FindDeadlockTest {

	private static final int DEADLOCK_TIMEOUT = 10*1000; /* CMVC 163829 - increase timeout to 10 seconds because opf nondeterminism in the test case */

	public static void main(String args[]) {
		new FindDeadlockTest().runTests();
	}

	static void printThreads(String title, Thread[] threads) {
		System.out.println(title);
		if (threads == null) {
			return;
		}
		for (int i = 0; i < threads.length; i++) {
			System.out.println("\t" + threads[i]);
		}
	}
	
	static void printThreads(String title, Thread[] threads, Object[] monitors) {
		System.out.println(title);
		if (threads == null) {
			return;
		}
		for (int i = 0; i < threads.length; i++) {
			System.out.println("\t" + threads[i] + "\t" + monitors[i]);
		}
	}

	static boolean matchThreads(Object[] got, Object[] expected) {
		class MatchRec {
			public Object o;
			public boolean found;
		}
	
		if (expected == null) {
			if (got != null) {
				return false;
			} else {
				return true;
			}
		}
	
		if (got == null) {
			return false;
		}
	
		if (got.length != expected.length) {
			return false;
		}
	
		MatchRec[] matches = new MatchRec[expected.length];
		for (int i = 0; i < matches.length; i++) {
			matches[i] = new MatchRec();
			matches[i].o = expected[i];
			matches[i].found = false;
		}
	
		for (int i = 0; i < got.length; i++) {
			boolean found = false;
	
			for (int j = 0; j < matches.length; j++) {
				if (matches[j].o == got[i]) {
					if (matches[j].found == true)
						return false;
	
					matches[j].found = true;
					found = true;
					break;
				}
			}
			if (!found)
				return false;
			found = false;
		}
	
		return true;
	}

	static boolean matchThreads(NativeHelpers.DeadlockList got, Thread[] expThreads, Object[] expObjects) {
		class MatchRec {
			public Object o;
			public boolean found;
		}

		if ((expThreads != null) && (expObjects != null)) {
			if (expThreads.length != expObjects.length) {
				throw new Error("matchThreads bad args");
			}
		} else {
			if ((expThreads == null) && (expObjects != null)) {
				throw new Error("matchThreads bad args");
			}
			if ((expThreads != null) && (expObjects == null)) {
				throw new Error("matchThreads bad args");
			}
		}
		
		if (got == null) {
			return false;
		}

		if (expThreads == null) {
			if (got.threads != null) {
				return false;
			} else {
				return true;
			}
		}
		
		if ((got.threads == null) || (got.monitors == null)) {
			return false;
		}

		if (got.threads.length != expThreads.length) {
			return false;
		}
		if (got.monitors.length != expObjects.length) {
			return false;
		}

		MatchRec[] matches = new MatchRec[expThreads.length];
		for (int i = 0; i < matches.length; i++) {
			matches[i] = new MatchRec();
			matches[i].o = expThreads[i];
			matches[i].found = false;
		}

		for (int i = 0; i < got.threads.length; i++) {
			boolean found = false;

			for (int j = 0; j < matches.length; j++) {
				if (matches[j].o == got.threads[i]) {
					if (matches[j].found == true)
						return false;
					if (!got.monitors[i].equals(expObjects[j]))
						return false;
					
					matches[j].found = true;
					found = true;
					break;
				}
			}
			if (!found)
				return false;
			found = false;
		}

		return true;
	}

	static boolean checkDeadlocks(Thread[] expThreads, Object[] expObjs) {
		boolean ok = true;
		
		Thread dthreads[] = NativeHelpers.findDeadlockedThreads();
		if (!FindDeadlockTest.matchThreads(dthreads, expThreads)) {
			ok = false;
		}
		
		NativeHelpers.DeadlockList list = new NativeHelpers.DeadlockList();
		NativeHelpers.findDeadlockedThreadsAndObjects(list);
		if (!FindDeadlockTest.matchThreads(list, expThreads, expObjs)) {
			ok = false;
		}
		
		if (!ok) {
			FindDeadlockTest.printThreads("findDeadlockedThreads returned", dthreads);
			FindDeadlockTest.printThreads("findDeadlockedThreadsAndObjects returned", list.threads, list.monitors);
		}
	
		return ok;
	}
	
	static void killThread(Thread thread) throws InterruptedException {
		thread.interrupt();
		thread.join();
	}

	public void runTests() {
		boolean ok = true;
		try {
			ok = ok && testNoDeadlocks();
			ok = ok && testDeadlocks();
		} catch (InterruptedException e) {
			e.printStackTrace();
		} catch (Throwable t) {
			/* Exceptions thrown while threads are deadlocked prevent VM shutdown.
			 * https://github.com/eclipse/openj9/issues/3369
			 * Force VM exit in this case.
			 */
			System.out.println("Unexpected exception:");
			t.printStackTrace();
			System.exit(-1);			
		}

		if (ok) {
			System.out.println("SUCCESS");
			System.exit(0);
		} else {
			System.out.println("THERE WERE TEST FAILURES");
			System.exit(-1);
		}
	}

	boolean testNoDeadlocks() throws InterruptedException {
		boolean ok = true;

		/* unstarted thread */
		Thread t_nostart = new Thread("nostart");

		/* threads that do nothing */
		Runnable r_sleep = new Runnable() {
			public void run() {
				try {
					while (true) {
						Thread.sleep(1000);
					}
				} catch (InterruptedException e) {
				}
			}
		};

		Thread t_sleep1 = new Thread(r_sleep, "sleep1");
		Thread t_sleep2 = new Thread(r_sleep, "sleep2");
		t_sleep1.start();
		t_sleep2.start();

		/* threads waiting for notification */
		Thread t_wait1 = new Thread("wait1") {
			public void run() {
				synchronized (this) {
					try {
						this.wait();
					} catch (InterruptedException e) {
					}
				}
			}
		};
		t_wait1.start();

		Thread t_wait2 = new Thread("wait2") {
			public void run() {
				synchronized (this) {
					try {
						this.wait();
					} catch (InterruptedException e) {
					}
				}
			}
		};
		t_wait2.start();

		/* Blocked threads */
		Object lock = new Object();
		Locker locker = new Locker("locker", lock);
		synchronized (locker) {
			locker.start();
			locker.wait();
		}

		Blocker b1 = new Blocker("b1", lock);
		Blocker b2 = new Blocker("b2", lock);
		Blocker b3 = new Blocker("b3", lock);

		b1.start();
		b2.start();
		b3.start();

		/* give some time for the blockers to run */
		Thread.sleep(2 * 1000);

		/* Chained blocked threads */
		Blocker c1 = new Blocker("c1", b1);
		Blocker c2 = new Blocker("c2", c1);
		Blocker c3 = new Blocker("c3", c2);

		c1.start();
		Thread.sleep(1000);
		c2.start();
		Thread.sleep(1000);
		c3.start();

		Thread.sleep(2 * 1000);

		/* find deadlocks */
		System.out.print("Testing no deadlocks...");
		ok = FindDeadlockTest.checkDeadlocks(null, null);
		System.out.println(ok? "Passed." : "Failed");

		// FindDeadlockTest.killThread(t_nostart);
		// FindDeadlockTest.killThread(t_sleep1);
		// FindDeadlockTest.killThread(t_sleep2);
		// FindDeadlockTest.killThread(t_wait1);
		// FindDeadlockTest.killThread(t_wait2);
		// FindDeadlockTest.killThread(locker);
		// FindDeadlockTest.killThread(b1);
		// FindDeadlockTest.killThread(b2);
		// FindDeadlockTest.killThread(b3);
		// FindDeadlockTest.killThread(c1);
		// FindDeadlockTest.killThread(c2);
		// FindDeadlockTest.killThread(c3);
		return ok;
	}

	boolean testDeadlocks() throws InterruptedException {
		boolean ok = true;

		Thread[] expected;
		Object[] monitors;

		/* simple deadlock */
		Object a = new Object();
		Object b = new Object();

		DThread d1 = new DThread("d1", a, b);
		DThread d2 = new DThread("d2", b, a);

		d1.start();
		d2.start();

		try {
			Thread.sleep(DEADLOCK_TIMEOUT);
		} catch (InterruptedException e) {
		}

		System.out.println("Testing simple deadlock...");
		expected = new Thread[2];
		expected[0] = d1;
		expected[1] = d2;
		monitors = new Object[2];
		monitors[0] = b;
		monitors[1] = a;
		
		if (!FindDeadlockTest.checkDeadlocks(expected, monitors)) {
			ok = false;
			System.out.println("Failed. Wrong threads found.");
		} else {
			System.out.println("Passed.");
		}

		/* two disjoint deadlocks */
		Object c = new Object();
		Object d = new Object();

		DThread d3 = new DThread("d3", c, d);
		DThread d4 = new DThread("d4", d, c);

		d3.start();
		d4.start();

		try {
			Thread.sleep(DEADLOCK_TIMEOUT);
		} catch (InterruptedException e) {
		}

		System.out.println("Testing disjoint simple deadlocks...");
		expected = new Thread[4];
		expected[0] = d1;
		expected[1] = d2;
		expected[2] = d3;
		expected[3] = d4;
		monitors = new Object[4];
		monitors[0] = b;
		monitors[1] = a;
		monitors[2] = d;
		monitors[3] = c;
		if (!FindDeadlockTest.checkDeadlocks(expected, monitors)) {
			System.out.println("Failed. Wrong deadlocked threads.");
			ok = false;
		} else {
			System.out.println("Passed.");
		}

		/* many-to-one deadlock */
		Object e = new Object();
		Object f = new Object();
		DThread d5 = new DThread("d5", e, a);
		DThread d6 = new DThread("d6", f, a);
		d5.start();
		d6.start();
		try {
			Thread.sleep(DEADLOCK_TIMEOUT);
		} catch (InterruptedException e1) {
			e1.printStackTrace();
		}
		System.out.println("Testing multiple deadlocks...");
		expected = new Thread[6];
		expected[0] = d1;
		expected[1] = d2;
		expected[2] = d3;
		expected[3] = d4;
		expected[4] = d5;
		expected[5] = d6;
		monitors = new Object[6];
		monitors[0] = b;
		monitors[1] = a;
		monitors[2] = d;
		monitors[3] = c;
		monitors[4] = a;
		monitors[5] = a;
		if (!FindDeadlockTest.checkDeadlocks(expected, monitors)) {
			System.out.println("Failed. Wrong deadlocked threads.");
			ok = false;
		} else {
			System.out.println("Passed.");
		}

		/* Chained deadlock */
		Blocker c1 = new Blocker("c1", a);
		Blocker c2 = new Blocker("c2", c1);
		Blocker c3 = new Blocker("c3", c2);
		Blocker c4 = new Blocker("c4", c3);
		c1.start();
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e2) {
			e2.printStackTrace();
		}
		c2.start();
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e2) {
			e2.printStackTrace();
		}
		c3.start();
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e2) {
			e2.printStackTrace();
		}
		c4.start();
		try {
			Thread.sleep(DEADLOCK_TIMEOUT);
		} catch (InterruptedException e1) {
		}
		System.out.println("Testing chained deadlocks...");
		expected = new Thread[10];
		expected[0] = d1;
		expected[1] = d2;
		expected[2] = d3;
		expected[3] = d4;
		expected[4] = d5;
		expected[5] = d6;
		expected[6] = c1;
		expected[7] = c2;
		expected[8] = c3;
		expected[9] = c4;
		monitors = new Object[10];
		monitors[0] = b;
		monitors[1] = a;
		monitors[2] = d;
		monitors[3] = c;
		monitors[4] = a;
		monitors[5] = a;
		monitors[6] = a;
		monitors[7] = c1;
		monitors[8] = c2;
		monitors[9] = c3;
		if (!FindDeadlockTest.checkDeadlocks(expected, monitors)) {
			System.out.println("Failed. Wrong deadlocked threads.");
			ok = false;
		} else {
			System.out.println("Passed.");
		}

		/* Indirect deadlock */
		DThread j1 = new DThread("j1");
		DThread j2 = new DThread("j2");
		DThread j3 = new DThread("j3");
		DThread j4 = new DThread("j4");

		j1.setFirst(j1);
		j1.setSecond(j2);
		j2.setFirst(j2);
		j2.setSecond(j3);
		j3.setFirst(j3);
		j3.setSecond(j4);
		j4.setFirst(j4);
		j4.setSecond(j1);

		j1.start();
		j2.start();
		j3.start();
		j4.start();

		try {
			Thread.sleep(DEADLOCK_TIMEOUT);
		} catch (InterruptedException e1) {
		}
		System.out.println("Testing indirect deadlocks...");
		expected = new Thread[14];
		expected[0] = d1;
		expected[1] = d2;
		expected[2] = d3;
		expected[3] = d4;
		expected[4] = d5;
		expected[5] = d6;
		expected[6] = c1;
		expected[7] = c2;
		expected[8] = c3;
		expected[9] = c4;
		expected[10] = j1;
		expected[11] = j2;
		expected[12] = j3;
		expected[13] = j4;
		monitors = new Object[14];
		monitors[0] = b;
		monitors[1] = a;
		monitors[2] = d;
		monitors[3] = c;
		monitors[4] = a;
		monitors[5] = a;
		monitors[6] = a;
		monitors[7] = c1;
		monitors[8] = c2;
		monitors[9] = c3;
		monitors[10] = j2;
		monitors[11] = j3;
		monitors[12] = j4;
		monitors[13] = j1;
		if (!FindDeadlockTest.checkDeadlocks(expected, monitors)) {
			System.out.println("Failed. Wrong deadlocked threads.");
			ok = false;
		} else {
			System.out.println("Passed.");
		}
		return ok;
	}

	class DThread extends Thread {
		Object first, second;

		DThread(String name) {
			super(name);
		}

		DThread(String name, Object first, Object second) {
			super(name);
			this.first = first;
			this.second = second;
		}

		public void setFirst(Object o) {
			this.first = o;
		}

		public void setSecond(Object o) {
			this.second = o;
		}

		public void run() {
			try {
				synchronized (first) {
					System.out.println(this + " locked " + first);
					for (int i = 0; i < 1; i++) {
						Thread.yield();
						Thread.sleep(1000);
						Thread.yield();
					}
					System.out.println(this + " locking " + second);
					synchronized (second) {
						System.out.println(this + " locked " + second);
					}
				}
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
	}

	class Blocker extends Thread {
		Object lock;

		public void run() {
			synchronized (this) {
				System.out.println("Blocker " + this + " locking " + lock);
				synchronized (lock) {
					System.out.println("Blocker " + this + " got " + lock);
				}
			}
		}

		Blocker(String name, Object lock) {
			super(name);
			this.lock = lock;
		}
	}

	class Locker extends Thread {
		Object lock;

		public void run() {
			try {
				synchronized (lock) {
					System.out.println("Locker got " + lock);
					synchronized (this) {
						this.notify();
					}
					while (true) {
						Thread.sleep(1000);
					}
				}
			} catch (InterruptedException e) {
			}
		}

		Locker(String name, Object lock) {
			super(name);
			this.lock = lock;
		}
	}
}
