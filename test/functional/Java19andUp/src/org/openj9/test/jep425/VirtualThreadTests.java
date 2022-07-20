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

import java.lang.Thread;
import java.time.Duration;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.stream.IntStream;

/**
 * Test cases for JEP 425: Virtual Threads (Preview) Continuation execution
 * which verifies the basic cases including Continuation enter, yield, resume.
 */
@Test(groups = { "level.sanity" })
public class VirtualThreadTests {

	@Test
	public void test_basicVirtualthread() {
		var wrapper = new Object(){ boolean executed = false; };
		try {
			Thread t = Thread.ofVirtual().name("duke").unstarted(()->{
				wrapper.executed = true;
			});

			t.start();
			t.join();

			AssertJUnit.assertTrue("Virtual Thread operation not executed", wrapper.executed);
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	@Test
	public void test_VirtualthreadYieldResume() {
		try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
			int[] results = new int[6];
			IntStream.range(0, 6).forEach(i -> {
					executor.submit(() -> {
							results[i] = 1;
							Thread.sleep(Duration.ofSeconds(1));
							results[i] += 1;
							return i;
					});
			});
			executor.awaitTermination(2L, TimeUnit.SECONDS);
			for (int i = 0; i < 6; i++) {
				AssertJUnit.assertTrue("Virtual Thread " + i + ": incorrect result of " + results[i], (results[i] == 2));
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}
}
