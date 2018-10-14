/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

package org.openj9.test.management.util;

import java.util.concurrent.CountDownLatch;

/**
 * This runs a busy loop for the time specified in milliseconds
 */
public class WorkerThread implements Runnable {
	long busyTime;
	private final CountDownLatch startLatch;
	private final CountDownLatch joinLatch;

	public WorkerThread(CountDownLatch startLatch, CountDownLatch joinLatch, int mSec) {
		busyTime = mSec * 1000000L;
		this.startLatch = startLatch;
		this.joinLatch = joinLatch;
	}

	public void run() {
		try {
			startLatch.await();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		long startTime = System.nanoTime();
		while ((System.nanoTime() - startTime) < busyTime) {
			/* Burn some CPU until the duration we are supposed to be "busy". */
			for (long i = 1; i < 1000000; i++) {
				quadratic(1, 2, 3, i);
			}
		}
		joinLatch.countDown();
	}

	public static long quadratic(long coeffA, long coeffB, long coeffC, long number) {
		return (coeffA * (number * number) + coeffB * number + coeffC);
	}

}
