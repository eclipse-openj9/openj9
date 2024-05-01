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
package org.openj9.test.java.lang;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity" })
public class Test_Thread_SM {

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

	Thread st;

	static boolean calledMySecurityManager = false;

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
			Assert.fail("CheckAccess failed", e);
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
				Assert.fail("Join failed ", e);
			}
		} finally {
			System.setSecurityManager(null);
		}
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

}
