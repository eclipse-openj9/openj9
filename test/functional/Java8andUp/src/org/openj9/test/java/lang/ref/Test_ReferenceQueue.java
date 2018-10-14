package org.openj9.test.java.lang.ref;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.openj9.test.support.Support_ExtendedTestEnvironment;
import org.testng.AssertJUnit;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;

@Test(groups = { "level.sanity" })
public class Test_ReferenceQueue {
	static Boolean b;
	static Integer integer;
	final boolean isJava8 = System.getProperty("java.specification.version").equals("1.8");
	final boolean disableClearBeforeEnqueue =
            Boolean.getBoolean("jdk.lang.ref.disableClearBeforeEnqueue");

	protected void doneSuite() {
		b = null;
		integer = null;
	}

	public class ChildThread implements Runnable {
		public ChildThread() {
		}

		public void run() {
			try {
				rq.wait(1000);
			} catch (Exception e) {
			}
			synchronized (rq) {
				// store in a static so it won't be gc'ed because the jit
				// optimized it out
				integer = new Integer(667);
				SoftReference sr = new SoftReference(integer, rq);
				sr.enqueue();
				rq.notify();
			}
			//notifyAll();

		}
	}

	ReferenceQueue rq;

	/**
	 * @tests java.lang.ref.ReferenceQueue#poll()
	 */
	@Test
	public void test_poll() {
		// store in a static so it won't be gc'ed because the jit
		// optimized it out
		b = new Boolean(true);
		//SM
		SoftReference sr = new SoftReference(b, rq);
		sr.enqueue();
		try {
			if (isJava8 || disableClearBeforeEnqueue) {
				AssertJUnit.assertTrue("Poll failed.", ((Boolean)rq.poll().get()).booleanValue());
			} else {
				AssertJUnit.assertTrue("Poll failed.", (rq.poll().get() == null));
			}
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during the test.", false);
		}
	}

	/**
	 * @tests java.lang.ref.ReferenceQueue#remove()
	 */
	@Test
	public void test_remove() {
		// store in a static so it won't be gc'ed because the jit
		// optimized it out
		b = new Boolean(true);
		//SM
		SoftReference sr = new SoftReference(b, rq);
		sr.enqueue();
		try {
			if (isJava8 || disableClearBeforeEnqueue) {
				AssertJUnit.assertTrue("Remove failed.", ((Boolean)rq.remove().get()).booleanValue());
			} else {
				AssertJUnit.assertTrue("Remove failed.", (rq.remove().get() == null));
			}
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during the test.", false);
		}
	}

	/**
	 * @tests java.lang.ref.ReferenceQueue#remove(long)
	 */
	@Test
	public void test_remove2() {
		//SM
		//There is something very unusual going on here. Remove(x) will always succeed,
		//no matter what x is.
		try {
			AssertJUnit.assertTrue("Queue is empty.", rq.poll() == null);
			AssertJUnit.assertTrue("Queue is empty.", rq.remove((long)1) == null);
			Thread ct = Support_ExtendedTestEnvironment.getInstance().getThread(new ChildThread());
			ct.start();
			//Thread.currentThread().sleep(100);
			Reference ret = rq.remove(0L);
			AssertJUnit.assertTrue("Delayed remove failed.", ret != null);
			//assertTrue("Delayed remove failed.", ((Integer)rq.remove(1L).get()).intValue()==667);
		} catch (InterruptedException e) {
			AssertJUnit.assertTrue("InterruptedExeException during test.", false);
		}

		catch (Exception e) {
			e.printStackTrace();
			AssertJUnit.assertTrue("Exception during test" + e.toString(), false);
		}
	}

	/**
	 * @tests java.lang.ref.ReferenceQueue#ReferenceQueue()
	 */
	@Test
	public void test_Constructor() {
		//SM.
		AssertJUnit.assertTrue("Used for testing.", true);
	}

	@BeforeMethod
	protected void setUp() {

		rq = new ReferenceQueue();
	}
}
