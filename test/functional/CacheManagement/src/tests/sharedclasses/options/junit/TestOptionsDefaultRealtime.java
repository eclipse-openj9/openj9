/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

package tests.sharedclasses.options.junit;

import java.util.HashSet;
import java.util.Set;

import tests.sharedclasses.TestUtils;

/**
 * For every testcase inherited from the superclass this will just let the cache file location default.  It will
 * also check for leaking memory/semaphores on unix platforms.
 */
public class TestOptionsDefaultRealtime extends TestOptionsBaseRealtime {

	Set sharedMemorySegments;
	Set sharedSemaphores;
	
	protected void setUp() throws Exception {
		super.setUp();
		if (System.getProperty("check.shared.memory","no").toLowerCase().equals("yes")) {
			if (!TestUtils.isWindows()  && !TestUtils.isMVS()) {
				sharedMemorySegments = TestUtils.getSharedMemorySegments();
				sharedSemaphores = TestUtils.getSharedSemaphores();	
			}
		}
		System.out.println("Running  :" + this.getName() + "  (Test suite : " + this.getClass().getSimpleName() + ")");
	}
	
	protected void tearDown() throws Exception {
		super.tearDown();
		TestUtils.runDestroyAllCaches();
		if (System.getProperty("check.shared.memory","no").toLowerCase().equals("yes")) {
			if (!TestUtils.isWindows() && !TestUtils.isMVS()) {
				Set after = TestUtils.getSharedMemorySegments();
				Set difference = new HashSet();
				difference.addAll(after);
				difference.removeAll(sharedMemorySegments);
				if (difference.size()!=0) {
					throw new RuntimeException("Shared memory leaked: "+difference.size());
				}
				after = TestUtils.getSharedSemaphores();
				difference = new HashSet();
				difference.addAll(after);
				difference.removeAll(sharedSemaphores);
				if (difference.size()!=0) {
					throw new RuntimeException("Shared semaphores leaked: "+difference.size());
				}
			}
		}
		TestUtils.runDestroyAllCaches();
	}
}
