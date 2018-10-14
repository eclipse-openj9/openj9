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
import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;

@Test(groups = { "level.sanity" })
public class Test_SoftReference {
	static Boolean bool;

	protected void doneSuite() {
		bool = null;
	}

	/**
	 * @tests java.lang.ref.SoftReference#SoftReference(java.lang.Object, java.lang.ref.ReferenceQueue)
	 */
	@Test
	public void test_Constructor() {
		//SM.
		ReferenceQueue rq = new ReferenceQueue();
		bool = new Boolean(true);
		try {
			SoftReference sr = new SoftReference(bool, rq);
			AssertJUnit.assertTrue("Initialization failed.", ((Boolean)sr.get()).booleanValue());
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during test.", false);
		}

		boolean exception = false;
		try {
			new SoftReference(bool, null);
		} catch (NullPointerException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("Should not throw NullPointerException", !exception);
	}

	/**
	 * @tests java.lang.ref.SoftReference#SoftReference(java.lang.Object)
	 */
	@Test
	public void test_Constructor2() {
		//SM.
		bool = new Boolean(true);
		try {
			SoftReference sr = new SoftReference(bool);
			AssertJUnit.assertTrue("Initialization failed.", ((Boolean)sr.get()).booleanValue());
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during test.", false);
		}
	}

	/**
	 * @tests java.lang.ref.SoftReference#get()
	 */
	@Test
	public void test_get() {
		//SM.
		bool = new Boolean(false);
		SoftReference sr = new SoftReference(bool);
		AssertJUnit.assertTrue("Same object not returned.", bool == sr.get());
	}

	/**
	 * @tests java.lang.ref.SoftReference#get()
	 */
	@Test
	public void test_get2() {
		SoftReference ref = new SoftReference(new Object());
		// if 90 gcs() doesn't remove the SoftReference, calling get() must be keeping it alive
		for (int i = 0; i < 90; i++) {
			ref.get();
			System.gc();
		}
		AssertJUnit.assertTrue("Reference is null", ref.get() != null);
	}
}
