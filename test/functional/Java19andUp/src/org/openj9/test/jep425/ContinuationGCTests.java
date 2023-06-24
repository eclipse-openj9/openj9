/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
package org.openj9.test.jep425;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import static org.testng.Assert.fail;

import java.util.concurrent.Executors;
import java.util.concurrent.Executor;
import java.util.stream.IntStream;
import java.lang.Thread;
import java.time.Duration;
import java.util.Random;

/**
 * Test cases for JEP 425: Virtual Threads (Preview) Continuation enter, yield, resume between GCs or during concurrent GCs
 */
@Test(groups = { "level.sanity" })
public class ContinuationGCTests {
	public void test_RunningGCTest() {
		boolean passed = false;
		var wrapper = new Object(){ boolean executed = false; };
		try {
			Thread t = Thread.ofVirtual().name("duke").unstarted(() -> {
				wrapper.executed = true;
			});

			t.start();
			t.join();
			AssertJUnit.assertTrue("Basical Virtual Thread operation not executed", wrapper.executed);

			Thread testThread = new Thread() {
				public void run() {
					try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
						IntStream.range(0, 50).forEach(i -> {
							executor.submit(() -> {
								Random rand = new Random();
								Thread.sleep(rand.nextInt(5));
								Thread.sleep(rand.nextInt(1000));
								allocateBytes();
								Thread.sleep(rand.nextInt(1000));
								if (0 == (i%5)) {
									Thread.sleep(rand.nextInt(2000));
									allocateBytes();
								}
								return i;
							});
						});
					}
				}
			};

			testThread.start();
			testThread.join();
			Thread.sleep(Duration.ofSeconds(2));
			allocateBytes();
			allocateBytes();

			passed = true;
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}

		AssertJUnit.assertTrue("ContinuationGCTest failed", passed);
	}

	private static void allocateBytes()
	{
		for (int cnt=0; cnt<1000; cnt++) {
			byte[] array640 = new byte[640];
			byte[] array1280 = new byte[1280];
			byte[] array2560 = new byte[2560];
			byte[] array5120 = new byte[5120];
		}
	}
}
