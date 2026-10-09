/*
 * Copyright IBM Corp. and others 2025
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
package com.ibm.dump.tests.javacore_vthread_monitor;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.locks.LockSupport;

/**
 * Creates a named virtual thread that enters a synchronized block (acquiring
 * a monitor) and then parks itself — causing the vthread to unmount while
 * still holding the monitor.  A javacore is then generated so that
 * CheckJavaCoreVThreadMonitor can verify:
 *
 *   1. The "Unmounted Threads" stack section shows a
 *      "5XESTACKTRACE (entered lock: ...)" line for the held monitor.
 *   2. The "Monitor pool" section names the vthread as owner rather than
 *      printing the anonymous "<detached virtual thread>" marker.
 */
public class CreateJavaCoreVThreadMonitor {

	/** Name of the virtual thread written into the javacore. */
	public static final String VTHREAD_NAME = "vthread-monitor-test";

	/** The lock object whose class name appears in the javacore lock lines. */
	static final Object LOCK = new Object();

	/** Signals that the vthread has entered the monitor and parked. */
	static final CountDownLatch READY = new CountDownLatch(1);

	public static void main(String[] args) throws Exception {
		Thread vthread = Thread.ofVirtual()
				.name(VTHREAD_NAME)
				.start(CreateJavaCoreVThreadMonitor::holdMonitorAndPark);

		/* Wait until the vthread is parked inside the synchronized block. */
		READY.await();

		/* Trigger the javacore while the vthread is unmounted. */
		com.ibm.jvm.Dump.JavaDump();

		/* Unblock the vthread so the JVM can exit cleanly. */
		LockSupport.unpark(vthread);
		vthread.join();
	}

	/**
	 * Entered by the virtual thread: acquires {@link #LOCK}, signals readiness,
	 * then parks (unmounting the vthread from its carrier).
	 */
	static void holdMonitorAndPark() {
		synchronized (LOCK) {
			READY.countDown();
			/* Park while inside the synchronized block so the monitor is held
			 * while the vthread is unmounted. */
			LockSupport.park();
		}
	}
}
