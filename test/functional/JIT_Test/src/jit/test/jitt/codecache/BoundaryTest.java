/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

package jit.test.jitt.codecache;

import jit.runner.JarTesterMT;
import junit.framework.TestCase;

/**
 * @author mesbah
 */
public class BoundaryTest extends TestCase {

	/**
	 * Boundary test for JIT code cache.
	 * Test�proper response when maximum code caches have been allocated and no space remains.
	 * 1) Set a small code cache size (128 kb)
	 * 2) Set small code size
	 * 3) Feed enough Jar files to JarTesterMT so that it can keep compiling classes until all code caches are filled
	 * 4) Validate that we reach the code cache max allocate message in the JIT verbose log
	 *
	 * NOTE: This test must not be run with any other code cache tests as it involves class compilation using a
	 * single class loader to minimize chance for method reclamation.
	 */

	@Override
	protected void setUp() throws Exception {
		super.setUp();
		TestManager.startLogParserInBackground( Constants.BOUNDARY_TEST );
		TestManager.startCompilationInBackground( false, true );	   // Boundary test does not require use of class unloading, so do not use multiple class loaders
	}

	@Override
	protected void tearDown() throws Exception {
		super.tearDown();
		boolean stoppedHarnessProcesses = TestManager.shutdownBackgroundProcesses();
		assertTrue(stoppedHarnessProcesses);
	}

	public void testBoundary() {

		JarTesterMT jarTesterMT = TestManager.getJarTesterMTDriver().getJartesterMT();
		LogParserThread logParserThread = TestManager.getLogParserDriver();

		//Wait for the class compilation to start
		while ( jarTesterMT.isStillCompiling() == false ) {
			try {
				Thread.sleep(2000);
			} catch ( InterruptedException e ) {
				e.printStackTrace();
			}
		}

		boolean pass = false;

		//While classes are still being compiled, sleep periodically and wake up to check if cache is full
		while ( jarTesterMT.isStillCompiling() ) {
			try {
				Thread.sleep(5000);
			} catch ( InterruptedException e ) {
				e.printStackTrace();
			}
			if ( logParserThread.isCodeCacheMaxAllocated() ) {
				pass = true;
				break;
			}
		}

		if( !pass ) {
			System.out.println("No more classes to load, code cache not full. Please try loading more classes");
		} else {
			System.out.println("Test : Cache Max Allocated : Passed");
		}

		assertTrue ( pass );
	}
}
