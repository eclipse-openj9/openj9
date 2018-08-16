
package org.openj9.test.java.lang;

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

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.ref.WeakReference;

@Test(groups = { "level.sanity" })
public class Test_Thread_Extra {

	/**
	 * @tests java.lang.Thread#destroy()
	 */
	@Test
	public void test_destroy() {
		try {
			new Thread().destroy();
			Assert.fail("NoSuchMethodError expected");
		} catch (NoSuchMethodError e) {
			// expected
		}
	}

	/**
	 * @tests java.lang.Thread#stop(java.lang.Throwable)
	 */
	@Test
	public void test_stop6() {
		try {
			new Thread().stop(new ThreadDeath());
			Assert.fail("should throw UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {
			// expected
		} catch (Throwable e) {
			Assert.fail("unexpected exception: " + e);
		}
	}

	/**
	 * Tears down the fixture, for example, close a network connection. This
	 * method is called after a test is executed.
	 */
	@AfterMethod
	protected void tearDown() {
		
		try {
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
