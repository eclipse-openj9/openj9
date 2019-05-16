/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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
package org.openj9.test.java.lang;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.ref.WeakReference;

@Test(groups = { "level.sanity" })
public class Test_Thread {

	static class SimpleThread implements Runnable {
		int delay;

		public void run() {
			try {
				synchronized (this) {
					this.notify();
					this.wait(delay);
				}
			} catch (InterruptedException e) {
				return;
			}

		}

		public SimpleThread(int d) {
			if (d >= 0)
				delay = d;
		}
	}

	static class YieldThread implements Runnable {
		volatile int delay;

		public void run() {
			int x = 0;
			while (true) {
				++x;
			}
		}

		public YieldThread(int d) {
			if (d >= 0)
				delay = d;
		}
	}

	static class ResSupThread implements Runnable {
		Thread parent;
		volatile int checkVal = -1;

		public void run() {
			try {
				synchronized (this) {
					this.notify();
				}
				while (true) {
					checkVal++;
					zz();
					Thread.sleep(100);
				}
			} catch (InterruptedException e) {
				return;
			} catch (BogusException e) {
				try {
					// Give parent a chance to sleep
					Thread.sleep(500);
				} catch (InterruptedException x) {
				}
				parent.interrupt();
				while (!Thread.currentThread().isInterrupted()) {
					// Don't hog the CPU
					try {
						Thread.sleep(50);
					} catch (InterruptedException x) {
						// This is what we've been waiting for...don't throw it
						// away!
						break;
					}
				}
			}
		}

		public void zz() throws BogusException {
		}

		public ResSupThread(Thread t) {
			parent = t;
		}

		public synchronized int getCheckVal() {
			return checkVal;
		}
	}

	static class BogusException extends Throwable {
		public BogusException(String s) {
			super(s);
		}
	}

	Thread st, ct, spinner;

	static boolean calledMySecurityManager = false;

	// used in test_defaultMemoryArea to pass status from thread started
	// back to thread running test case
	boolean pass = true;

	/**
	 * @tests java.lang.Thread#Thread()
	 */
	@Test
	public void test_Constructor() {
		Thread t;
		SecurityManager m = new SecurityManager() {
			public ThreadGroup getThreadGroup() {
				calledMySecurityManager = true;
				return Thread.currentThread().getThreadGroup();
			}
		};
		try {
			System.setSecurityManager(m); // To see if it checks Thread creation
			// with our SecurityManager

			t = new Thread();
		} finally {
			System.setSecurityManager(null); // restore original, no
			// side-effects
		}
		AssertJUnit.assertTrue("Did not call SecurityManager.getThreadGroup ()", calledMySecurityManager);
		t.start();
	}

	/**
	 * @tests java.lang.Thread#Thread(java.lang.Runnable)
	 */
	@Test
	public void test_Constructor2() {
		try {
			ct = new Thread(new SimpleThread(10));
			ct.start();
		} catch (Exception e) {
			AssertJUnit.assertTrue("Failed to create subthread", false);
		}
	}

	/**
	 * @tests java.lang.Thread#Thread(java.lang.Runnable, java.lang.String)
	 */
	@Test
	public void test_Constructor3() {
		Thread st1 = new Thread(new SimpleThread(1), "SimpleThread1");
		AssertJUnit.assertTrue("Constructed thread with incorrect thread name", st1.getName().equals("SimpleThread1"));
		st1.start();
	}

	/**
	 * @tests java.lang.Thread#Thread(java.lang.String)
	 */
	@Test
	public void test_Constructor4() {
		Thread t = new Thread("Testing");
		AssertJUnit.assertTrue("Created tread with incorrect name", t.getName().equals("Testing"));
		t.start();
	}

	/**
	 * @tests java.lang.Thread#Thread(java.lang.ThreadGroup, java.lang.Runnable)
	 */
	@Test
	public void test_Constructor5() {
		ThreadGroup tg = new ThreadGroup("Test Group1");
		st = new Thread(tg, new SimpleThread(1), "SimpleThread2");
		AssertJUnit.assertTrue("Returned incorrect thread group", st.getThreadGroup() == tg);
		st.start();
		try {
			st.join();
		} catch (InterruptedException e) {
		}
		tg.destroy();
	}

	/**
	 * @tests java.lang.Thread#Thread(java.lang.ThreadGroup, java.lang.Runnable,
	 *        java.lang.String)
	 */
	@Test
	public void test_ConstructorLjava_lang_ThreadGroup6() {
		ThreadGroup tg = new ThreadGroup("Test Group2");
		st = new Thread(tg, new SimpleThread(1), "SimpleThread3");
		AssertJUnit.assertTrue("Constructed incorrect thread",
				(st.getThreadGroup() == tg) && st.getName().equals("SimpleThread3"));
		st.start();
		try {
			st.join();
		} catch (InterruptedException e) {
		}
		tg.destroy();

		Runnable r = new Runnable() {
			public void run() {
			}
		};

		ThreadGroup foo = null;
		try {
			new Thread(foo = new ThreadGroup("foo"), r, null);
			// Should not get here
			AssertJUnit.assertTrue("Null cannot be accepted as Thread name", false);
		} catch (NullPointerException npe) {
			AssertJUnit.assertTrue("Null cannot be accepted as Thread name", true);
			foo.destroy();
		}

	}

	/**
	 * @tests java.lang.Thread#Thread(java.lang.ThreadGroup, java.lang.String)
	 */
	@Test
	public void test_Constructor6() {
		st = new Thread(new SimpleThread(1), "SimpleThread4");
		AssertJUnit.assertTrue("Returned incorrect thread name", st.getName().equals("SimpleThread4"));
		st.start();
	}

	/**
	 * @tests java.lang.Thread#activeCount()
	 */
	@Test
	public void test_activeCount() {
		Thread t = new Thread(new SimpleThread(1));
		int active = t.activeCount();
		AssertJUnit.assertTrue("Incorrect read made: " + active, active > 0);
		t.start();
		try {
			t.join();
		} catch (InterruptedException e) {
		}
	}

	/**
	 * @tests java.lang.Thread#checkAccess()
	 */
	@Test
	public void test_checkAccess() {
		ThreadGroup tg = new ThreadGroup("Test Group3");
		try {
			st = new Thread(tg, new SimpleThread(1), "SimpleThread5");
			st.checkAccess();
			AssertJUnit.assertTrue("CheckAccess passed", true);
		} catch (SecurityException e) {
			AssertJUnit.assertTrue("CheckAccess failed", false);
		}
		st.start();
		try {
			st.join();
		} catch (InterruptedException e) {
		}
		tg.destroy();
	}

	/**
	 * @tests java.lang.Thread#checkAccess_interrupt_self()
	 */
	@Test
	public void test_checkAccess_interrupt_self() {
		try {
			System.setSecurityManager(new SecurityManager() {
				public void checkAccess(Thread t) {
					Assert.fail("CheckAccess() was invoked");
				}
			});

			Thread.currentThread().interrupt();
			AssertJUnit.assertTrue("Failed to interrupt current thread", Thread.interrupted());
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.Thread#checkAccess_interrupt_not_self()
	 */
	@Test
	public void test_checkAccess_interrupt_not_self() {
		try {
			SimpleThread simple = new SimpleThread(5000);

			st = new Thread(simple);

			synchronized (simple) {
				st.start();

				try {
					simple.wait();
				} catch (InterruptedException e) {
				}
			}

			class MySecurityManager extends SecurityManager {
				public volatile boolean called = false;

				public void checkAccess(Thread t) {
					called = true;
				}
			}

			MySecurityManager sm = new MySecurityManager();

			System.setSecurityManager(sm);

			AssertJUnit.assertTrue("Thread should be alive", st.isAlive());

			st.interrupt();

			AssertJUnit.assertTrue("checkAccess() was not called", sm.called);

			try {
				st.join(10000);
			} catch (InterruptedException e) {
				Assert.fail("Join failed ");
			}
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.Thread#countStackFrames()
	 */
	@Test
	public void test_countStackFrames() {
		AssertJUnit.assertTrue("Test failed.", Thread.currentThread().countStackFrames() == 0);
	}

	/**
	 * @tests java.lang.Thread#currentThread()
	 */
	@Test
	public void test_currentThread() {
		// Test for method java.lang.Thread java.lang.Thread.currentThread()
		AssertJUnit.assertTrue("Current thread was not main thread: " + Thread.currentThread().getName(),
				Thread.currentThread().getName().equals("main"));
	}

	/**
	 * @tests java.lang.Thread#enumerate(java.lang.Thread[])
	 */
	@Test
	public void test_enumerate() {
		class EnumerateHelper extends Thread implements Runnable {
			Thread[] tarray = new Thread[10];
			int enumerateCount;

			EnumerateHelper(ThreadGroup group) {
				super(group, "Enumerate Helper");
			}

			public synchronized int getEnumerateCount() {
				return enumerateCount;
			}

			public synchronized void run() {
				try {
					this.notify();
					while (true) {
						this.wait();
						enumerateCount = Thread.enumerate(tarray);
						this.notify();
					}
				} catch (InterruptedException ex) {
				}
			}
		}

		ThreadGroup mytg = new ThreadGroup("test_enumerate");
		EnumerateHelper enumerateHelper = new EnumerateHelper(mytg);
		synchronized (enumerateHelper) {
			enumerateHelper.start();
			try {
				enumerateHelper.wait();
			} catch (InterruptedException e) {
				Assert.fail("Unexpected interrupt 2");
			}
		}

		SimpleThread st1 = new SimpleThread(-1);
		SimpleThread st2 = new SimpleThread(-1);
		Thread firstOne = new Thread(mytg, st1, "firstOne2");
		Thread secondOne = new Thread(mytg, st2, "secondOne1");
		synchronized (enumerateHelper) {
			enumerateHelper.notify();
			try {
				enumerateHelper.wait();
			} catch (InterruptedException e) {
				Assert.fail("Unexpected interrupt 2");
			}
			int newCount = enumerateHelper.getEnumerateCount();
			AssertJUnit.assertEquals("initial count should be 1", 1, newCount);
		}
		synchronized (st1) {
			firstOne.start();
			try {
				st1.wait();
			} catch (InterruptedException e) {
			}
		}
		synchronized (enumerateHelper) {
			enumerateHelper.notify();
			try {
				enumerateHelper.wait();
			} catch (InterruptedException e) {
				Assert.fail("Unexpected interrupt 2");
			}
			int newCount = enumerateHelper.getEnumerateCount();
			AssertJUnit.assertEquals("Simple Thread 1 not added to enumeration", 2, newCount);
		}

		synchronized (st2) {
			secondOne.start();
			try {
				st2.wait();
			} catch (InterruptedException e) {
			}
		}
		synchronized (enumerateHelper) {
			enumerateHelper.notify();
			try {
				enumerateHelper.wait();
			} catch (InterruptedException e) {
				Assert.fail("Unexpected interrupt 4");
			}
			int newCount = enumerateHelper.getEnumerateCount();
			AssertJUnit.assertEquals("Simple Thread 2 not added to enumeration", 3, newCount);
		}

		synchronized (st1) {
			firstOne.interrupt();
		}
		synchronized (st2) {
			secondOne.interrupt();
		}
		synchronized (enumerateHelper) {
			enumerateHelper.interrupt();
		}
		try {
			firstOne.join();
			secondOne.join();
			enumerateHelper.join();
		} catch (InterruptedException e) {
		}
		mytg.destroy();
	}

	/**
	 * @tests java.lang.Thread#getContextClassLoader()
	 */
	@Test
	public void test_getContextClassLoader() {
		Thread t = new Thread();
		AssertJUnit.assertTrue("Incorrect class loader returned",
				t.getContextClassLoader() == Thread.currentThread().getContextClassLoader());
		t.start();

		/* [PR CMVC 90230] behavior change in 1.5 */
		// behavior change in 1.5, Thread constructors should call
		// parentThread.getContextClassLoader()
		// instead of accessing field
		final ClassLoader loader = new ClassLoader() {
		};
		class ContextThread extends Thread {
			private ClassLoader contextClassLoader;

			public ClassLoader getContextClassLoader() {
				return contextClassLoader;
			}

			public void setContextClassLoader(ClassLoader loader) {
				contextClassLoader = loader;
			}

			public void run() {
				Thread thread = new Thread();
				ClassLoader contextLoader = thread.getContextClassLoader();
				AssertJUnit.assertTrue("incorrect context class loader", contextLoader == loader);
			}
		}
		;
		Thread sub = new ContextThread();
		sub.setContextClassLoader(loader);
		sub.start();
		/* [PR CMVC 90230] behavior change in 1.5 */
		// behavior change in 1.5, Thread constructors should check
		// enableContextClassLoaderOverride
		// if thread overrides getContextClassLoader() or
		// setContextClassLoader() */
		class SubContextThread extends ContextThread {
		}
		;
		System.setSecurityManager(new SecurityManager());
		try {
			// creating a new Thread, or subclass of Thread should not cause an
			// exception
			new Thread();
			new Thread() {
			};
			boolean exception = false;
			try {
				new SubContextThread();
			} catch (SecurityException e) {
				AssertJUnit.assertTrue("wrong exception: " + e,
						e.getMessage().indexOf("enableContextClassLoaderOverride") != -1);
				exception = true;
			}
			AssertJUnit.assertTrue("exception not thrown", exception);
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.Thread#getName()
	 */
	@Test
	public void test_getName() {
		st = new Thread(new SimpleThread(1), "SimpleThread6");
		AssertJUnit.assertTrue("Returned incorrect thread name", st.getName().equals("SimpleThread6"));
		st.start();
	}

	/**
	 * @tests java.lang.Thread#getPriority()
	 */
	@Test
	public void test_getPriority() {
		st = new Thread(new SimpleThread(1));
		st.setPriority(Thread.MAX_PRIORITY);
		AssertJUnit.assertTrue("Returned incorrect thread priority", st.getPriority() == Thread.MAX_PRIORITY);
		st.start();
	}

	/**
	 * @tests java.lang.Thread#getThreadGroup()
	 */
	@Test
	public void test_getThreadGroup() {
		ThreadGroup tg = new ThreadGroup("Test Group4");
		st = new Thread(tg, new SimpleThread(1), "SimpleThread8");
		AssertJUnit.assertTrue("Returned incorrect thread group", st.getThreadGroup() == tg);
		st.start();
		try {
			st.join();
		} catch (InterruptedException e) {
		}
		/* [PR 97317] A dead thread should return a null thread group */
		AssertJUnit.assertTrue("group should be null", st.getThreadGroup() == null);
		AssertJUnit.assertTrue("toString() should not be null", st.toString() != null);
		tg.destroy();

		/* [PR CMVC 88976] the Thread is alive until cleanup() is called */
		final Object lock = new Object();
		Thread t = new Thread() {
			public void run() {
				synchronized (lock) {
					lock.notifyAll();
				}
			}
		};
		synchronized (lock) {
			t.start();
			try {
				lock.wait();
			} catch (InterruptedException e) {
			}
		}
		int running = 0;
		while (t.isAlive())
			running++;
		ThreadGroup group = t.getThreadGroup();
		AssertJUnit.assertTrue("ThreadGroup is not null", group == null);
	}

	/**
	 * @tests java.lang.Thread#interrupt()
	 */
	@Test
	public void test_interrupt() {
		final Object lock = new Object();
		class ChildThread1 extends Thread {
			Thread parent;
			boolean sync;

			public void run() {
				if (sync) {
					synchronized (lock) {
						lock.notify();
						try {
							lock.wait();
						} catch (InterruptedException e) {
						}
					}
				}
				parent.interrupt();
			}

			public ChildThread1(Thread p, String name, boolean sync) {
				parent = p;
				this.sync = sync;
			}
		}
		boolean interrupted = false;
		try {
			ct = new ChildThread1(Thread.currentThread(), "Interrupt Test1", false);
			synchronized (lock) {
				ct.start();
				lock.wait();
			}
		} catch (InterruptedException e) {
			interrupted = true;
		}
		AssertJUnit.assertTrue("Failed to Interrupt thread1", interrupted);

		interrupted = false;
		try {
			ct = new ChildThread1(Thread.currentThread(), "Interrupt Test2", true);
			synchronized (lock) {
				ct.start();
				lock.wait();
				lock.notify();
			}
			Thread.sleep(20000);
		} catch (InterruptedException e) {
			interrupted = true;
		}
		AssertJUnit.assertTrue("Failed to Interrupt thread2", interrupted);

	}

	/**
	 * @tests java.lang.Thread#interrupted()
	 */
	@Test
	public void test_interrupted() {
		AssertJUnit.assertTrue("Interrupted returned true for non-interrupted thread", Thread.interrupted() == false);
		Thread.currentThread().interrupt();
		AssertJUnit.assertTrue("Interrupted returned true for non-interrupted thread", Thread.interrupted() == true);
		AssertJUnit.assertTrue("Failed to clear interrupted flag", Thread.interrupted() == false);
	}

	/**
	 * @tests java.lang.Thread#isAlive()
	 */
	@Test
	public void test_isAlive() {
		SimpleThread simple;
		st = new Thread(simple = new SimpleThread(500));
		AssertJUnit.assertTrue("Unstarted thread returned true", !st.isAlive());
		synchronized (simple) {
			st.start();
			try {
				simple.wait();
			} catch (InterruptedException e) {
			}
		}
		AssertJUnit.assertTrue("Started thread returned false", st.isAlive());
		try {
			st.join();
		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("Thread did not die", false);
		}
		AssertJUnit.assertTrue("Stopped thread returned true", !st.isAlive());
	}

	/**
	 * @tests java.lang.Thread#isDaemon()
	 */
	@Test
	public void test_isDaemon() {
		st = new Thread(new SimpleThread(1), "SimpleThread10");
		AssertJUnit.assertTrue("Non-Daemon thread returned true", !st.isDaemon());
		st.setDaemon(true);
		AssertJUnit.assertTrue("Daemon thread returned false", st.isDaemon());
		st.start();
	}

	/**
	 * @tests java.lang.Thread#isInterrupted()
	 */
	@Test
	public void test_isInterrupted() {
		class SpinThread implements Runnable {
			public volatile boolean done = false;

			public void run() {
				while (!Thread.currentThread().isInterrupted())
					;
				while (!done)
					;
			}
		}

		SpinThread spin = new SpinThread();
		spinner = new Thread(spin);
		spinner.start();
		Thread.yield();
		try {
			AssertJUnit.assertTrue("Non-Interrupted thread returned true", !spinner.isInterrupted());
			spinner.interrupt();
			AssertJUnit.assertTrue("Interrupted thread returned false", spinner.isInterrupted());
			spin.done = true;
		} finally {
			spinner.interrupt();
			spin.done = true;
		}
	}

	/**
	 * @tests java.lang.Thread#join()
	 */
	@Test
	public void test_join() {
		SimpleThread simple;
		try {
			st = new Thread(simple = new SimpleThread(100));
			AssertJUnit.assertTrue("Thread is alive", !st.isAlive()); // cause isAlive() to
			// be compiled by
			// the jit, as it
			// must be called
			// within 100ms
			// below
			synchronized (simple) {
				st.start();
				simple.wait();
			}
			st.join();
		} catch (InterruptedException e) {
			Assert.fail("Join failed ");
		}
		AssertJUnit.assertTrue("Joined thread is still alive", !st.isAlive());
		boolean result = true;
		Thread th = new Thread("test");
		try {
			// synchronized(lock) {
			// killer.start();
			// lock.wait();
			// }
			th.join();
		} catch (InterruptedException e) {
			result = false;
		}
		// killer.interrupt();
		AssertJUnit.assertTrue("Hung joining a non-started thread", result);
		th.start();
	}

	/**
	 * @tests java.lang.Thread#join(long)
	 */
	@Test
	public void test_join2() {
		SimpleThread simple;
		try {
			st = new Thread(simple = new SimpleThread(1000), "SimpleThread12");
			AssertJUnit.assertTrue("Thread is alive", !st.isAlive()); // cause isAlive() to
			// be compiled by
			// the jit, as it
			// must be called
			// within 100ms
			// below
			synchronized (simple) {
				st.start();
				simple.wait();
			}
			st.join(10);
		} catch (InterruptedException e) {
			Assert.fail("Join failed ");
		}
		AssertJUnit.assertTrue("Join failed to timeout", st.isAlive());

		st.interrupt();
		try {
			st = new Thread(simple = new SimpleThread(100), "SimpleThread13");
			synchronized (simple) {
				st.start();
				simple.wait();
			}
			st.join(1000);
		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("Join failed ", false);
			return;
		}
		AssertJUnit.assertTrue("Joined thread is still alive", !st.isAlive());

		final Object lock = new Object();
		final Thread main = Thread.currentThread();
		Thread killer = new Thread(new Runnable() {
			public void run() {
				try {
					synchronized (lock) {
						lock.notify();
					}
					Thread.sleep(100);
				} catch (InterruptedException e) {
					return;
				}
				main.interrupt();
			}
		});
		boolean result = true;
		Thread th = new Thread("test");
		try {
			synchronized (lock) {
				killer.start();
				lock.wait();
			}
			th.join(200);
		} catch (InterruptedException e) {
			result = false;
		}
		killer.interrupt();
		AssertJUnit.assertTrue("Hung joining a non-started thread", result);
		th.start();
	}

	/**
	 * @tests java.lang.Thread#join(long, int)
	 */
	@Test
	public void test_join3() {
		SimpleThread simple;
		try {
			Thread t = new Thread(simple = new SimpleThread(3000), "Squawk1");
			synchronized (simple) {
				t.start();
				simple.wait();
			}
			long firstRead = System.currentTimeMillis();
			t.join(100, 999999);
			long secondRead = System.currentTimeMillis();
			AssertJUnit.assertTrue("Did not join by appropriate time: " + secondRead + "-" + firstRead + "="
					+ (secondRead - firstRead), secondRead - firstRead >= 100);
			AssertJUnit.assertTrue("Joined thread is not alive", t.isAlive());
			t.interrupt();
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during test: " + e.toString(), false);
		}

		final Object lock = new Object();
		final Thread main = Thread.currentThread();
		Thread killer = new Thread(new Runnable() {
			public void run() {
				try {
					synchronized (lock) {
						lock.notify();
					}
					Thread.sleep(100);
				} catch (InterruptedException e) {
					return;
				}
				main.interrupt();
			}
		});
		boolean result = true;
		Thread th = new Thread("test");
		try {
			synchronized (lock) {
				killer.start();
				lock.wait();
			}
			th.join(2000, 20);
		} catch (InterruptedException e) {
			result = false;
		}
		killer.interrupt();
		AssertJUnit.assertTrue("Hung joining a non-started thread", result);
		th.start();
	}

	/**
	 * @tests java.lang.Thread#resume()
	 */
	@Test
	public void test_resume() {
		int orgval;
		ResSupThread t;
		try {
			t = new ResSupThread(Thread.currentThread());
			synchronized (t) {
				ct = new Thread(t, "Interupt Test2");
				ct.start();
				t.wait();
			}
			ct.suspend();
			// Wait to be sure the suspend has occurred
			Thread.sleep(500);
			orgval = t.getCheckVal();
			// Wait to be sure the thread is suspended
			Thread.sleep(500);
			AssertJUnit.assertTrue("Failed to suspend thread", orgval == t.getCheckVal());
			ct.resume();
			// Wait to be sure the resume has occurred.
			Thread.sleep(500);
			AssertJUnit.assertTrue("Failed to resume thread", orgval != t.getCheckVal());
			ct.interrupt();
		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("Unexpected interrupt occurred", false);
		}
	}

	/**
	 * @tests java.lang.Thread#run()
	 */
	@Test
	public void test_run() {
		class RunThread implements Runnable {
			boolean didThreadRun = false;

			public void run() {
				didThreadRun = true;
			}
		}
		RunThread rt = new RunThread();
		Thread t = new Thread(rt);
		try {
			t.start();
			int count = 0;
			while (!rt.didThreadRun && count < 20) {
				Thread.sleep(100);
				count++;
			}
			AssertJUnit.assertTrue("Thread did not run", rt.didThreadRun);
			t.join();
		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("Joined thread was interrupted", true);
		}
		AssertJUnit.assertTrue("Joined thread is still alive", !t.isAlive());
	}

	/**
	 * @tests java.lang.Thread#setDaemon(boolean)
	 */
	@Test
	public void test_setDaemon() {
		st = new Thread(new SimpleThread(1), "SimpleThread14");
		st.setDaemon(true);
		AssertJUnit.assertTrue("Failed to set thread as daemon thread", st.isDaemon());
		st.start();
	}

	/**
	 * @tests java.lang.Thread#setName(java.lang.String)
	 */
	@Test
	public void test_setName() {
		st = new Thread(new SimpleThread(1), "SimpleThread15");
		st.setName("Bogus Name");
		AssertJUnit.assertTrue("Failed to set thread name", st.getName().equals("Bogus Name"));
		try {
			st.setName(null);
			AssertJUnit.assertTrue("Null should not be accepted as a valid name", false);
		} catch (NullPointerException e) {
			// success
			AssertJUnit.assertTrue("Null should not be accepted as a valid name", true);
		}
		st.start();
	}

	/**
	 * @tests java.lang.Thread#setPriority(int)
	 */
	@Test
	public void test_setPriority() {
		st = new Thread(new SimpleThread(1));
		st.setPriority(Thread.MAX_PRIORITY);
		AssertJUnit.assertTrue("Failed to set priority", st.getPriority() == Thread.MAX_PRIORITY);
		st.start();
	}

	/**
	 * @tests java.lang.Thread#sleep(long)
	 */
	@Test
	public void test_sleep() {
		// BB Test is somewhat bogus.
		long stime = 0, ftime = 0;
		try {
			stime = System.currentTimeMillis();
			Thread.currentThread().sleep(1000);
			ftime = System.currentTimeMillis();
		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("Unexpected interrupt received", false);
		}
		AssertJUnit.assertTrue("Failed to sleep long enough", (ftime - stime) >= 800);
	}

	/**
	 * @tests java.lang.Thread#start()
	 */
	@Test
	public void test_start() {
		try {
			ResSupThread t = new ResSupThread(Thread.currentThread());
			synchronized (t) {
				ct = new Thread(t, "Interrupt Test4");
				ct.start();
				t.wait();
			}
			AssertJUnit.assertTrue("Thread is not running1", ct.isAlive());
			// Let the child thread get going.
			int orgval = t.getCheckVal();
			/* [PR CMVC 133063] timing related test failure */
			for (int i = 0; i < 5; i++) {
				Thread.sleep(150);
				if (orgval != t.getCheckVal())
					break;
			}
			AssertJUnit.assertTrue("Thread is not running2", orgval != t.getCheckVal());
			ct.interrupt();
		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("Unexpected interrupt occurred", false);
		}
	}

	/**
	 * @tests java.lang.Thread#start()
	 */
	@Test
	public void test_start2() {
		final ThreadGroup myGroup = new ThreadGroup("group1");

		class TestThreadStart extends Thread {
			String testResult = null;

			public TestThreadStart(ThreadGroup group, String threadName) {
				super(group, threadName);
			}

			@Override
			public void run() {
				try {
					/* Since this thread has already running, calling start of this thread should throw IllegalThreadStateException */
					this.start();
					testResult = "Expected IllegalThreadStateException is not thrown";
					Assert.fail(testResult);
				} catch (IllegalThreadStateException e) {
					/* IllegalThreadStateException is expected
					 *
					 *  This thread's group should only have this thread as child since it is alive and still running.
					 */
					if (myGroup.activeCount() != 1) {
						testResult = "This running thread's thread group does not have one child which should be this thread";
						Assert.fail(testResult);
					}
				}
			}

			public String getTestResult() {
				return testResult;
			}
		}

		TestThreadStart testThrStart = new TestThreadStart(myGroup, "testThreadStart");
		testThrStart.start();
		try {
			testThrStart.join();
			myGroup.destroy();
			AssertJUnit.assertTrue(testThrStart.getTestResult(), testThrStart.getTestResult() == null);
		} catch (InterruptedException e) {
			myGroup.destroy();
			AssertJUnit.assertTrue("Unexpected interrupt occurred", false);
		}
	}

	/**
	 * @tests java.lang.Thread#start()
	 * @tests java.lang.ref.WeakReference#WeakReference(java.lang.Object)
	 */
	@Test
	public void test_start_WeakReference() {
		/* [PR CVMC 118827] references are not removed in dead threads */
		Object o = new Object();
		final WeakReference ref = new WeakReference(o);
		Thread t = new Thread(new Runnable() {
			// If Thread.runnable isn't cleared, "save" holds a reference
			Object save;
			ThreadLocal tl = new ThreadLocal();
			InheritableThreadLocal itl = new InheritableThreadLocal();

			public void run() {
				save = ref.get();
				tl.set(save);
				itl.set(save);
			}
		});
		t.start();
		try {
			t.join();
		} catch (InterruptedException e) {
		}
		o = null;
		System.gc();
		AssertJUnit.assertTrue("runnable was not collected", ref.get() == null);
		// must keep a reference to the Thread so it doesn't go out of scope
		AssertJUnit.assertTrue("null Thread", t != null);
	}

	/**
	 * @tests java.lang.Thread#stop()
	 */
	@Test
	public void test_stop() {
		try {
			Runnable r = new ResSupThread(null);
			synchronized (r) {
				st = new Thread(r, "Interupt Test5");
				st.start();
				r.wait();
			}

		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("Unexpected interrupt received", false);
		}
		st.stop();

		/*
		 * Thread.stop() implementation is changed from 1.1.7 to the 1.2
		 * behaviour. now stop() doesn't ensure immediate thread death, so we
		 * have to wait till st thread joins back to current thread, before
		 * making an isAlive() check.
		 */
		try {
			st.join(10000);
		} catch (InterruptedException e1) {
			st.interrupt();
			Assert.fail("Failed to stopThread before 10000 timeout");
		}
		AssertJUnit.assertTrue("Failed to stopThread", !st.isAlive());
	}

	/**
	 * @tests java.lang.Thread#stop()
	 */
	@Test
	public void test_stop2() {
		/* [PR CMVC 94728] AccessControlException on dead Thread */
		Thread t = new Thread("t");
		class MySecurityManager extends SecurityManager {
			public boolean intest = false;

			public void checkAccess(Thread t) {
				if (intest) {
					Assert.fail("checkAccess called");
				}
			}
		}
		MySecurityManager sm = new MySecurityManager();
		System.setSecurityManager(sm);
		try {
			sm.intest = false;
			t.start();
			try {
				t.join(2000);
			} catch (InterruptedException e) {
			}
			sm.intest = true;
			try {
				t.stop();
				// Ignore any SecurityExceptions, may not have stopThread
				// permission
			} catch (SecurityException e) {
			}
			sm.intest = false;
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.Thread#stop(java.lang.Throwable)
	 */
	@Test
	public void test_stop5() {
		class StopBeforeStartThread extends Thread {
			public boolean failed = false;

			public void run() {
				synchronized (this) {
					failed = true;
				}
			}
		}
		try {
			StopBeforeStartThread t = new StopBeforeStartThread();
			t.stop();
			t.start();
			t.join();
			synchronized (t) {
				AssertJUnit.assertFalse("thread should not run if stop called before start, stop()", t.failed);
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception:" + e);
		}
	}

	/**
	 * @tests java.lang.Thread#suspend()
	 */
	@Test
	public void test_suspend() {
		int orgval;
		ResSupThread t = new ResSupThread(Thread.currentThread());
		try {
			synchronized (t) {
				ct = new Thread(t, "Interupt Test6");
				ct.start();
				t.wait();
			}
			ct.suspend();
			// Wait to be sure the suspend has occurred
			Thread.sleep(500);
			orgval = t.getCheckVal();
			// Wait to be sure the thread is suspended
			Thread.sleep(500);
			AssertJUnit.assertTrue("Failed to suspend thread", orgval == t.getCheckVal());
			ct.resume();
			// Wait to be sure the resume has occurred.
			Thread.sleep(500);
			AssertJUnit.assertTrue("Failed to resume thread", orgval != t.getCheckVal());
			ct.interrupt();
		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("Unexpected interrupt occurred", false);
		}

		/*
		 * [PR 106321] suspend() must not synchronize when a Thread is
		 * suspending itself
		 */
		final Object notify = new Object();
		Thread t1 = new Thread(new Runnable() {
			public void run() {
				synchronized (notify) {
					notify.notify();
				}
				Thread.currentThread().suspend();
			}
		});
		try {
			synchronized (notify) {
				t1.start();
				notify.wait();
			}
			// wait for Thread to suspend
			Thread.sleep(500);
			AssertJUnit.assertTrue("Thread should be alive", t1.isAlive());
			t1.resume();
			t1.join();
		} catch (InterruptedException e) {
		}
	}

	/**
	 * @tests java.lang.Thread#toString()
	 */
	@Test
	public void test_toString() {
		ThreadGroup tg = new ThreadGroup("Test Group5");
		st = new Thread(tg, new SimpleThread(1), "SimpleThread17");
		final String stString = st.toString();
		final String expected = "Thread[SimpleThread17,5,Test Group5]";
		AssertJUnit.assertTrue("Returned incorrect string: " + stString + "\t(expecting :" + expected + ")",
				stString.equals(expected));
		st.start();
		try {
			st.join();
		} catch (InterruptedException e) {
		}
		tg.destroy();
	}

	/**
	 * Tears down the fixture, for example, close a network connection. This
	 * method is called after a test is executed.
	 */
	@AfterMethod
	protected void tearDown() {
		try {
			if (st != null)
				st.interrupt();
		} catch (Exception e) {
		}
		try {
			if (spinner != null)
				spinner.interrupt();
		} catch (Exception e) {
		}
		try {
			if (ct != null)
				ct.interrupt();
		} catch (Exception e) {
		}

		try {
			spinner = null;
			st = null;
			ct = null;
			System.runFinalization();
		} catch (Exception e) {
		}

		/*Make sure that the current thread is not interrupted. If it is, set it back to false.*/
		/*Otherwise any call to join, wait, sleep from the current thread throws either InterruptedException or IllegalMonitorStateException*/
		if (Thread.currentThread().isInterrupted()) {
			try {
				Thread.currentThread().join(10);
			} catch (InterruptedException e) {
				//If we are here, current threads interrupted status is set to false.
			}
		}
	}
}
