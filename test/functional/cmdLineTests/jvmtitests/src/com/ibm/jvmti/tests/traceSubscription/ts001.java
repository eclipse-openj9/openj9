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
package com.ibm.jvmti.tests.traceSubscription;

public class ts001 {
	/**
	 * Test the extended function jvmtiRegisterTraceSubscriber()
	 * 
	 *  
	 * @return true on pass
	 */

	private static final byte eyecatcherASCII[] = new byte[] { 'U', 'T', 'T', 'H' };
	private static final byte eyecatcherEBCDIC[] = new byte[] { (byte)228, (byte)227, (byte)227, (byte)200 };	
	
	public String helpTraceSubscription()
	{
		return "Check trace subscriber functionality. " +
		       "Added as a unit test for J9 VM design ID 1944";		
	}

	public boolean testTraceSubscription()
	{
		int count = 0;
		
		System.out.println("Subscribing to trace");
		if (!tryRegisterTraceSubscriber()) {
			System.err.println("Failed to subscribe to trace");
			return false;
		}

		System.out.println("Flushing trace data");
		if (!tryFlushTraceData()) {
			System.err.println("Failed to flush trace data");
			return false;
		}
		

		System.out.println("waiting for buffers to be passed to subscriber");
		while (getBufferCount() <= 0) {
			try {
				/* we are throwing this exception to try and generate trace data
				 * so that buffers will be passed to the test subscription
				 */
				throw new Exception("generate trace data");
			} catch (Exception e) {
			}
		}

		System.out.println("Unsubscribing from trace");
		count = tryDeregisterTraceSubscriber();
		
		if (count == -1) {
			System.err.println("Failed to deregister subscriber");
			return false;
		}

		/* should fail with no external trace because no way to flush in core trace */
		System.out.println("Flushing trace data (will fail if not outputing trace to a file)");
		try {
			tryFlushTraceData();
		} catch (Exception e) {
			/* suppress jvmti invalid env exception */
		}

		/* wait for alarm to complete */
		System.out.println("waiting for alarm to complete");
		while (getFinalBufferCount() == 0) {
			try {
				Thread.sleep(1000);
			} catch (Exception e) {
				/* suppress */
			}
		}

		/* check that buffer count hasn't incremented since alarm completed */
		System.out.println("Checking that buffer count hasn't increased after unsubscribe completed");
		if (getBufferCount() != getFinalBufferCount()) {
			System.err.println("Buffer count increased after subscriber deregistered");
			return false;
		}

		
		/* lastly, make sure we can get the metadata that's needed to process the trace records */
		byte eyecatcher[] = new byte[4];
		for (int i = 0; i < eyecatcher.length; i++) {
			/* gets all the metadata and returns the i'th  byte */
			byte b = tryGetTraceMetadata(i);
			eyecatcher[i] =  b;
			if (b != eyecatcherASCII[i] && b != eyecatcherEBCDIC[i]) {
				System.err.println("eyecatcher byte in trace metadata at index "+i+" doesn't match expectation. Found: '"+(char)b+"'");
				return false;
			}
		}
		
		System.out.println("Trace metadata validated");
		return true;		
	}

	public static native boolean tryRegisterTraceSubscriber();
	public static native boolean tryFlushTraceData();
	public static native int tryDeregisterTraceSubscriber();
	public static native int getBufferCount();
	public static native int getFinalBufferCount();
	public static native byte tryGetTraceMetadata(int index);
}
