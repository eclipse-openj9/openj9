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
import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;

import org.testng.Assert;
import org.testng.annotations.Test;

/* Verify that the returned stack has the correct depth */
public class StackDepth extends ThreadMXBeanTestCase {
	@Test(groups = { "level.extended" })
	public void testStackDepth() {
		int maxDepth = 50;
		int[] depths = { -1, 0, 1, 4, 20, 60, Integer.MAX_VALUE };
		Thread t = new RecursiveThread(this, maxDepth);
		ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
		ThreadInfo ti;

		try {
			/* wait for the recursive thread to run */
			synchronized (this) {
				t.start();
				this.wait();
			}

			for (int i = 0; i < depths.length; i++) {
				try {
					ti = mxb.getThreadInfo(t.getId(), depths[i]);
					if (ti.getThreadId() != t.getId()) {
						logger.error("FAILED. Wrong thread.");
						this.setFailed();
					}
					if (depths[i] > maxDepth + 4) {
						if (ti.getStackTrace().length != maxDepth + 4) {
							logger.debug("wanted: " + depths[i] + " got: " + ti.getStackTrace().length);
						}
					} else if (ti.getStackTrace().length != depths[i]) {
						logger.error("wanted: " + depths[i] + " got: " + ti.getStackTrace().length);
						this.setFailed();
					}
				} catch (IllegalArgumentException e) {
					if (depths[i] >= 0) {
						logger.error("wanted: " + depths[i] + " got: -1");
						this.setFailed();
					}
				}
			}

			/* let the recursive thread finish */
			synchronized (this) {
				this.notify();
			}
			t.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
			this.setFailed();
		}

		Assert.assertEquals(getExitStatus(), ExitStatus.PASS);
	}

	class RecursiveThread extends Thread {
		int maxDepth = 1;

		Object sync;

		RecursiveThread(Object sync, int maxDepth) {
			this.sync = sync;
			this.maxDepth = maxDepth;
		}

		void recurse(int depth) {
			if (depth > maxDepth) {
				synchronized (sync) {
					sync.notify();
					try {
						sync.wait();
					} catch (InterruptedException e) {
						/* do nothing, just exit */
					}
				}
			} else {
				depth++;
				recurse(depth);
			}
		}

		public void run() {
			recurse(0);
		}
	}
}
