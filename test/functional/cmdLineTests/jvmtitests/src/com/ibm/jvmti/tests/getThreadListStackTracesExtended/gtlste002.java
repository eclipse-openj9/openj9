/*
 * Copyright IBM Corp. and others 2024
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
package com.ibm.jvmti.tests.getThreadListStackTracesExtended;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executors;
import java.util.concurrent.locks.LockSupport;

public class gtlste002 {
	static long ii = 0;
	static final int iter = 1000;
	static int reachedIter = 0;

	/**
	 * Check if there is at least one JITted frame in the stack trace of the given thread.
	 * @param t the thread to be checked, if t is null, the current thread will be used
	 * @return true if there is at least one JITted frames in the stack trace of the given thread
	 */
	public static native boolean anyJittedFrame(Thread t);

	/**
	 * Method to be compiled in the tests. If the virtual thread will be unmounted, the
	 * return value should be ignored as it will be checked separately.
	 * @param mounted if the tested virtual thread will be mounted or not
	 * @return true if there is at least one JITted frames in the stack trace of the virtual thread.
	 */
	boolean methodToCompile(boolean mounted) {
		for (int j = 0; j < 1000; j++) {
			ii++;
		}

		if (reachedIter == iter) {
			if (mounted) {
				return anyJittedFrame(null);
			} else {
				LockSupport.park();
			}
		}
		return true;
	}

	/**
	 * Returns the help message of testUnmountedVthread() which will be printed by the test suite.
	 * @return the help message of testUnmountedVthread()
	 */
	public String helpUnmountedVthread() {
		return "Tests jvmtiGetThreadListStackTracesExtended for at least 1 jitted frame after a relatively large number of iterations on unmounted virtual threads.";
	}

	/**
	 * Tests jvmtiGetThreadListStackTracesExtended for at least 1 jitted frame after a relatively large number of iterations on unmounted virtual threads.
	 * @return true if the test passes
	 */
	public boolean testUnmountedVthread() {
		boolean rc = false;
		reachedIter = 0;

		try {
			Thread t1 = Thread.ofVirtual().name("vthread 1").start(() -> {
				while (reachedIter < iter) {
					reachedIter++;
					methodToCompile(false);
				}
			});

			while (t1.getState() != Thread.State.WAITING) {
				Thread.sleep(500);
			}
			rc = anyJittedFrame(t1);

			LockSupport.unpark(t1);

			t1.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}

		return rc;
	}

	/**
	 * Returns the help message of testMountedVthread() which will be printed by the test suite.
	 * @return the help message of testMountedVthread()
	 */
	public String helpMountedVthread() {
		return "Tests jvmtiGetThreadListStackTraceExtended for at least 1 jitted frame after a relatively large number of iterations on mounted virtual threads.";
	}

	/**
	 * Tests jvmtiGetThreadListStackTracesExtended for at least 1 jitted frame after a relatively large number of iterations on mounted virtual threads.
	 * @return true if the test passes
	 */
	public boolean testMountedVthread() {
		boolean rc = false;
		reachedIter = 0;

		try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
			var futureResult = executor.submit(() -> {
				boolean res = false;
				while (reachedIter < iter) {
					reachedIter++;
					boolean ret = methodToCompile(true);
					if (reachedIter == iter) {
						res = ret;
					}
				}
				return res;
			});
			rc = futureResult.get();
		} catch (InterruptedException | ExecutionException e) {
			e.printStackTrace();
		}

		return rc;
	}
}
