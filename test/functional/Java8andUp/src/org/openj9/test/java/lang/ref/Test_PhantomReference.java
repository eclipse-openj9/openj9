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
import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;

@Test(groups = { "level.sanity" })
public class Test_PhantomReference {

	static Boolean bool;

	protected void doneSuite() {
		bool = null;
	}

	/**
	 * @tests java.lang.ref.PhantomReference#get()
	 */
	@Test
	public void test_get() {
		ReferenceQueue rq = new ReferenceQueue();
		bool = new Boolean(false);
		PhantomReference pr = new PhantomReference(bool, rq);
		AssertJUnit.assertTrue("Same object returned.", pr.get() == null);
	}

	/**
	 * @tests java.lang.ref.PhantomReference#PhantomReference(java.lang.Object, java.lang.ref.ReferenceQueue)
	 */
	@Test
	public void test_Constructor() {
		ReferenceQueue rq = new ReferenceQueue();
		bool = new Boolean(true);
		try {
			PhantomReference pr = new PhantomReference(bool, rq);
			// Allow the finalizer to run to potentially enqueue
			Thread.sleep(1000);
			AssertJUnit.assertTrue("Initialization failed.", !pr.isEnqueued());
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during test.", false);
		}
		// need a reference to bool so the jit does not optimize it away
		AssertJUnit.assertTrue("should always pass", bool.booleanValue());

		boolean exception = false;
		try {
			new PhantomReference(bool, null);
		} catch (NullPointerException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("Should not throw NullPointerException", !exception);
	}
}
