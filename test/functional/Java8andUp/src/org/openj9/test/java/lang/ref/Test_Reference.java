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
import org.testng.AssertJUnit;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;
import java.lang.ref.WeakReference;

@Test(groups = { "level.sanity" })
public class Test_Reference {
	Object tmpA, tmpB, obj;
	volatile WeakReference wr;
	final boolean isJava8 = System.getProperty("java.specification.version").equals("1.8");
	final boolean disableClearBeforeEnqueue =
            Boolean.getBoolean("jdk.lang.ref.disableClearBeforeEnqueue");

	protected void doneSuite() {
		tmpA = tmpB = obj = null;
	}

	/**
	 * @tests java.lang.ref.Reference#clear()
	 */
	@Test
	public void test_clear() {
		tmpA = new Object();
		tmpB = new Object();
		SoftReference sr = new SoftReference(tmpA, new ReferenceQueue());
		WeakReference wr = new WeakReference(tmpB, new ReferenceQueue());
		AssertJUnit.assertTrue("Start: Object not cleared.", (sr.get() != null) && (wr.get() != null));
		sr.clear();
		wr.clear();
		AssertJUnit.assertTrue("End: Object cleared.", (sr.get() == null) && (wr.get() == null));
		// Must reference tmpA and tmpB so the jit does not optimize them away
		AssertJUnit.assertTrue("should always pass", tmpA != sr.get() && tmpB != wr.get());
	}

	/**
	 * @tests java.lang.ref.Reference#enqueue()
	 */
	@Test
	public void test_enqueue() {
		ReferenceQueue rq = new ReferenceQueue();
		obj = new Object();
		Reference ref = new SoftReference(obj, rq);
		AssertJUnit.assertTrue("Enqueue failed.", (!ref.isEnqueued()) && ((ref.enqueue()) && (ref.isEnqueued())));
		if (isJava8 || disableClearBeforeEnqueue) {
			AssertJUnit.assertTrue("Not properly enqueued.", rq.poll().get() == obj);
		} else {
			AssertJUnit.assertTrue("Not properly enqueued.", rq.poll().get() == null);
		}
		
		AssertJUnit.assertTrue("Should remain enqueued.", !ref.isEnqueued()); //This fails.
		AssertJUnit.assertTrue("Can not enqueue twice.", (!ref.enqueue()) && (rq.poll() == null));

		rq = new ReferenceQueue();
		obj = new Object();
		ref = new WeakReference(obj, rq);
		AssertJUnit.assertTrue("Enqueue failed2.", (!ref.isEnqueued()) && ((ref.enqueue()) && (ref.isEnqueued())));
		if (isJava8 || disableClearBeforeEnqueue) {
			AssertJUnit.assertTrue("Not properly enqueued2.", rq.poll().get() == obj);
		} else {
			AssertJUnit.assertTrue("Not properly enqueued2.", rq.poll().get() == null);
		}
		AssertJUnit.assertTrue("Should remain enqueued2.", !ref.isEnqueued()); //This fails.
		AssertJUnit.assertTrue("Can not enqueue twice2.", (!ref.enqueue()) && (rq.poll() == null));
	}

	/**
	 * @tests java.lang.ref.Reference#get()
	 */
	@Test
	public void test_get() {
		//SM.
		obj = new Object();
		Reference ref = new WeakReference(obj, new ReferenceQueue());
		AssertJUnit.assertTrue("Get succeeded.", ref.get() == obj);
	}

	/**
	 * @tests java.lang.ref.Reference#isEnqueued()
	 */
	@Test
	public void test_isEnqueued() {
		//SM.
		ReferenceQueue rq = new ReferenceQueue();
		obj = new Object();
		Reference ref = new SoftReference(obj, rq);
		AssertJUnit.assertTrue("Should start off not enqueued.", !ref.isEnqueued());
		ref.enqueue();
		AssertJUnit.assertTrue("Should now be enqueued.", ref.isEnqueued());
		ref.enqueue();
		AssertJUnit.assertTrue("Should still be enqueued.", ref.isEnqueued());
		rq.poll();
		AssertJUnit.assertTrue("Should now be not enqueued.", !ref.isEnqueued()); //This fails.
	}
}
