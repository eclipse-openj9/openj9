/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.java.lang.management.ThreadMXBean;

import java.lang.management.*;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

public class MaxMonitorsTest extends ThreadMXBeanTestCase {
	ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
	volatile int threadcount = 0;
	LockClass barrier1 = new LockClass("barrier1");
	LockClass barrier2 = new LockClass("barrier2");
	final int stackDepth = 101;

	@Test(groups = { "level.extended" })
	public void testMaxMonitorsTest() {
		int maxthreads = 0;
		ThreadInfo[] ti = null;
		
		/* the max dumpable threads is limited by the number of local refs */
		int maxdump = 65 * 1024 / (6 + stackDepth * 3);

		try {

			synchronized (barrier1) {
				synchronized (barrier2) {
					try {
						while (true) {
							Thread th = new BigStackThread("t" + maxthreads, stackDepth);
							th.start();
							maxthreads++;
							if (maxthreads > maxdump) break;
						}
					} catch (OutOfMemoryError e) {
					}
					logger.debug(maxthreads + " threads allocated");

					threadcount = maxthreads;
					while (threadcount > 0) {
						barrier1.wait();
					}
					threadcount = maxthreads;
					Thread.currentThread().setPriority(Thread.MAX_PRIORITY);
					
					while (threadcount > maxdump) {
						barrier2.wait();
					}
					
					boolean done = false;
					while (!done) {
						try {
							logger.debug("attempting to dump: " + threadcount);
							ti = mxb.dumpAllThreads(true, true);
							done = true;
						} catch (OutOfMemoryError e) {
							if (threadcount > 0) {
								barrier2.wait();
							}
						}
					}
					
					while (threadcount > 0) {
						barrier2.wait();
					}
				}
			}
			
			logger.debug("threads dumped: " + ti.length);
			printThreadInfo(ti);
		} catch (InterruptedException e) {
		}
	}

	class BigStackThread extends Thread {
		int maxStackDepth;

		BigStackThread(String name, int maxStackDepth) {
			super(name);
			this.maxStackDepth = maxStackDepth;
		}

		private synchronized void func(int stackDepth) {
			if (stackDepth >= maxStackDepth) {
				synchronized (barrier1) {
					threadcount--;
					barrier1.notify();
				}

				synchronized (barrier2) {
					threadcount--;
					barrier2.notify();
				}
			} else {
				LockClass lock = new LockClass(null);
				synchronized (lock) {
					func(stackDepth + 1);
				}
			}
		}

		public void run() {
			func(0);
		}
	}

	class LockClass {
		String name;

		LockClass(String name) {
			this.name = name;
		}
	}
}
