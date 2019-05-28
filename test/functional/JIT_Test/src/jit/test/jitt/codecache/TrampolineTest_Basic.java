/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import junit.framework.TestCase;

/**
 * @author mesbah
 *
 */
public class TrampolineTest_Basic extends TestCase {

	@Override
	protected void setUp() throws Exception {
		super.setUp();
		TestManager.startLogParserInBackground( Constants.BASIC_TEST );
		//Turn off using multiple ZipTester compilation threads
		TestManager.startCompilationInBackground( true, false );
	}

	@Override
	protected void tearDown() throws Exception {
		super.tearDown();
		boolean stoppedHarnessProcesses = TestManager.shutdownBackgroundProcesses();
		//assertTrue(stoppedHarnessProcesses);
	}

	/**
	 * Basic trampoline tests where code caches are sufficiently distant from each other,
	 * and inter-code cache dispatches are made.
	 */
	public void testTrampolineBasic(){
		try {
			/*ensure that callee() method gets loaded to an initial code cache*/
			/*call caller1() and callee() twice so that they get compiled*/
			Caller1_BT caller1 = new Caller1_BT();
			int result = caller1.caller();
			result = caller1.caller();
			assertEquals(1,result);

			/*Sleep so that the background compilation process can compile arbitrary classes now.
			 *We sleep until we have filled up at least 10 code caches since resumption of the background class compiler thread */
			int currentCodeCacheIndex = TestManager.getLogParserDriver().getCurrentCodeCacheIndex();
			while ( true ) {
				Thread.sleep(4000);
				if ( TestManager.getLogParserDriver().getCurrentCodeCacheIndex() > currentCodeCacheIndex + 10 ) {
					break;
				}
			}

			/*Load caller2(). At this point, caller2() should be loaded in a distant code cache than callee().
			 *So, when caller2() invokes callee(), trampoline is used now */
			Caller2_BT caller2 = new Caller2_BT();
			result = caller2.caller();
			result = caller2.caller();

			/*Validate that callee() is still returning correct result*/
			assertEquals(4,result);

			/*Validate that callee() and caller2 are on different code caches*/

			/*wait a little bit so that the compilation info for caller2 gets printed to log and read in by the LogParserThread */
			Thread.sleep(6000);

			int codeCacheDistance = TestManager.getLogParserDriver().getCodeCacheDifference(
					Constants.BASIC_TRAMPOLINE_TEST_METHOD_SIG_CALLEE,
					Constants.BASIC_TRAMPOLINE_TEST_METHOD_SIG_CALLER2);

			if ( codeCacheDistance < 0 ) {
				/*May be we did not wait long enough for the LogParserThread to update itself with info on caller 2, so wait and try one more time*/
				for ( int i = 0 ; i < 4 ; i++ ){
					Thread.sleep(6000);
					codeCacheDistance = TestManager.getLogParserDriver().getCodeCacheDifference(
							Constants.BASIC_TRAMPOLINE_TEST_METHOD_SIG_CALLEE,
							Constants.BASIC_TRAMPOLINE_TEST_METHOD_SIG_CALLER2);
					if ( codeCacheDistance > 0 ) {
						break;
					}
				}
			}

			System.out.println("Code Cache distance between callee() and caller() = " + codeCacheDistance);

			/*Validate that callee() and caller() are not in same code cache, confirms use of trampoline(?) */
			assertTrue ( codeCacheDistance > 0 );

		} catch ( Exception e ) {
			e.printStackTrace();
			fail(e.getMessage());
		}
	}
}
