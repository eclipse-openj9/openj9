/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package com.ibm.jvmti.tests.getOwnedMonitorInfo;

public class gomi002
{	
	static Object lock = new Object();
	static boolean running = false;
	static volatile boolean stop = false;
	
	native static void callGet(Thread t);
	
	public boolean testForeignThread() throws Throwable
	{
		Thread runner = new Thread() {
			public long recurse(int i, long v) {
				if (i > 0) {
					v = System.currentTimeMillis() + recurse(i - 1, v);
				}
				return v;
			}
			public void run() {
				synchronized(lock) {
					running = true;
					lock.notifyAll();
				}
				while (!stop) {
					recurse(100, 0);
				}
			}
		};		

		runner.start();
		synchronized(lock) {
			while(!running) {
				lock.wait();
			}
		}

		long start = System.currentTimeMillis();
		while((System.currentTimeMillis() - start) < 5000) {
			callGet(runner);
			Thread.sleep(100);
		}

		stop = true;
		runner.join();
		
		return true;
	}
		
	public String helpForeignThread()
	{
		return "test GetOwnedMonitorInfo on non-current thread which is running";
	}
}
