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
package jit.test.finalizer;

import java.lang.management.ManagementFactory;
import java.util.Hashtable;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;

public class TestObject {
	private static final Logger logger = Logger.getLogger(TestObject.class);

	public static Hashtable isInstanceFinalized = new Hashtable();
	private String name;
	private static final long waitTime = 360000;
	private static final long sleepTime = 2000;
	
	public TestObject(String name) {
		isInstanceFinalized.put(name, Boolean.valueOf(false));
		this.name = name;
	}
	
	/* (non-Javadoc)
	 * @see java.lang.Object#finalize()
	 * The overridden finalize() method sets the flag to true.
	 */
	@Override
	protected void finalize() throws Throwable {
		super.finalize();
		isInstanceFinalized.put(name, Boolean.valueOf(true));
	}

	/**
	 * Fake some work
	 */
	public void doWork() {
		logger.info("TestObject working!");
		logger.info("Done!");
	}

	public static boolean isFinalized(String name) {
		Boolean wrapper = (Boolean) isInstanceFinalized.get(name);
		if(wrapper == null)
			throw new Error("FinalizationIndicator instance for " + name + " does not exist");
		return wrapper.booleanValue();
	}
	
	protected static void thoroughGCandFinalization() {
		System.gc();
		System.gc();
		System.runFinalization();
		System.runFinalization();
		System.gc();
		System.gc();
		System.runFinalization();
		System.runFinalization();
	}
	
	@Test(groups = { "level.sanity","component.jit" })
	public static void testFinalizer() {
		String objectName = "test";
		TestObject test = new TestObject(objectName);
		test.doWork();
		test = null;
		long iterationLimit = waitTime / sleepTime;		
		
		while (0 != iterationLimit) {
			thoroughGCandFinalization();
			try {
				Thread.sleep(sleepTime);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			if (isFinalized(objectName)) {
				break;
			}
			iterationLimit--;
		}
				
		if (isFinalized(objectName)) {
			logger.info("PASSED:FINALIZER_INVOKED");
		} else {
			if (ManagementFactory.getMemoryMXBean().getObjectPendingFinalizationCount() > 0) {
				Assert.fail("FAILED:FINALIZER_NOT_INVOKED"); // object is in the pending finalization queue, the finalizer is still waiting to be invoked.
			} else {
				Assert.fail("FAILED:FINALIZER_NOT_PENDED"); // object is not in the pending queue yet
			}
		}
	}
}
