/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2016 IBM Corp. and others
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
package com.ibm.lang.management.internal;

/**
 * A thread hooked as a VM shutdown hook to tell a MemoryNotificationThread
 * to terminate.
 *
 * @since 1.5
 */
final class MemoryNotificationThreadShutdown extends Thread {

	private final MemoryNotificationThread myVictim;

	/**
	 * Basic constructor
	 * @param victim The thread to notify on shutdown
	 */
	MemoryNotificationThreadShutdown(MemoryNotificationThread victim) {
		super();
		myVictim = victim;
	}

	/**
	 * Shutdown hook code that coordinates the termination of a memory
	 * notification thread.
	 */
	@Override
	public void run() {
		sendShutdownNotification();
		try {
			// wait for the notification thread to terminate
			myVictim.join();
		} catch (InterruptedException e) {
			// don't care
		}
	}

	/**
	 * Wipes any pending notifications and puts a shutdown request
	 * notification on an internal notification queue.
	 */
	private native void sendShutdownNotification();

}
