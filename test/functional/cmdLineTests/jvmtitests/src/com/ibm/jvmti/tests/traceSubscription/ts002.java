/*******************************************************************************
 * Copyright (c) 2013, 2013 IBM Corp. and others
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

public class ts002 {
	/**
	 * Test the JVMTI extension functions jvmtiRegisterTracePointSubscriber() and jvmtiDeregisterTracePointSubscriber()
	 */

	public String helpTracePointSubscription()
	{
		return "Check tracepoint subscriber functionality. " +
		       "Added as a unit test for J9 VM design Jazz103 #40425";
	}

	public boolean testTracePointSubscription()
	{
		int count = 0;

		System.out.println("Subscribing to selected -Xtrace tracepoints");
		if (!tryRegisterTracePointSubscribers()) {
			System.err.println("Failed to subscribe to tracepoints");
			return false;
		}

		System.out.println("Generating tracepoints to be passed to the subscribers");
		while (getTracePointCount1() < 50 || getTracePointCount2() < 50) {
			try {
				/* we are throwing this exception to generate trace data */
				throw new Exception("generate trace data");
			} catch (Exception e) {
			}
		}

		/* wait for alarms to complete */
		System.out.println("Checking that subscriber alarms have been called");
		while (hasAlarmTriggered1() == false || hasAlarmTriggered2() == false) {
			try {
				Thread.sleep(500);
			} catch (Exception e) {
				/* suppress */
			}
		}

		System.out.println("Unsubscribing from -Xtrace tracepoints");
		if (!tryDeregisterTracePointSubscribers()) {
			System.err.println("Failed to unsubscribe to tracepoints");
			return false;
		}

		return true;
	}

	public static native boolean tryRegisterTracePointSubscribers();
	public static native boolean tryDeregisterTracePointSubscribers();
	public static native int getTracePointCount1();
	public static native int getTracePointCount2();
	public static native boolean hasAlarmTriggered1();
	public static native boolean hasAlarmTriggered2();
}
