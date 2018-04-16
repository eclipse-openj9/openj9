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

import junit.framework.TestCase;

/**
 * @author mesbah
 *
 */
public class TrampolineTest_Advanced extends TestCase {

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
	 * Advanced trampoline tests where a recompiled body lies in a distant
	 * code cache relative to the original code cache requiring a trampoline
	 * where one wasn't required before
	 */
	public void testTrampolineAdvanced(){
		try {
			/*Ensure that caller1() and callee() methods get loaded to a same initial code cache*/


			/*Let background compilation threads to get started first*/
			Thread.sleep(4000);

			/*Pause background compilation*/
			TestManager.pauseCompilation();

			/*call callee() twice through caller1() so that both get compiled */
			Caller1_AT caller1 = new Caller1_AT();
			int result = caller1.caller();
			result = caller1.caller();
			assertEquals(1,result);

			/*Wait a little bit so that the compilation info for callee() and caller1() gets printed to log and read in by the LogParserThread */
			Thread.sleep(4000);

			/*Validate that caller1() and callee() have been compiled into the same code cache*/

			int initialCCDistance = TestManager.getLogParserDriver().getCodeCacheDifference(
					Constants.ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLEE,
					Constants.ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLER);

			if ( initialCCDistance < 0 ) {
				/*May be we did not wait long enough for the LogParserThread to update itself with info on caller1 and callee, so wait and try one more time*/
				for ( int i = 0 ; i < 4 ; i++ ){
					Thread.sleep(4000);
					initialCCDistance = TestManager.getLogParserDriver().getCodeCacheDifference(
							Constants.ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLEE,
							Constants.ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLER);
					if ( initialCCDistance > -1 ) {
						break;
					}
				}
			}

			/*Initially callee() and caller1() must be in the same code cache which ensures we have not used trampoline yet*/
			assertEquals ( 0, initialCCDistance ) ;

			System.out.println("Initial code cache distance between callee() and caller1() = " + initialCCDistance);

			/*Resume background compilation*/
			TestManager.resumeCompilation();

			/*Sleep so that the background compilation process can compile arbitrary classes now
			 * We sleep until we have filled up at least 10 code caches since resumption of the background class compiler thread*/
			int currentCodeCacheIndex = TestManager.getLogParserDriver().getCurrentCodeCacheIndex();
			while ( true ) {
				Thread.sleep(4000);
				if ( TestManager.getLogParserDriver().getCurrentCodeCacheIndex() > currentCodeCacheIndex + 10 ) {
					break;
				}
			}

			/*Load caller2() which invokes callee() with a higher workload prompting JIT to recompile callee()
			 * Once callee() is recompiled, it should be at least 10 code caches away from caller1()*/
			Caller2_AT caller2 = new Caller2_AT();
			result = caller2.caller();
			result = caller2.caller();

			/*Now that callee() has been recompiled and placed in a different code cache, invoke caller1() which in turns calls callee()
			 * This time trampoline must be used*/
			result = caller1.caller();

			/*Validate we still get correct result*/
			assertEquals( 1, result );

			/*Validate that callee() and caller1 are on different code caches*/

			/*Wait a little bit so that the compilation info for caller1 gets printed to log and read in by the LogParserThread */
			Thread.sleep(6000);

			int finalCCDistance = TestManager.getLogParserDriver().getCodeCacheDifference(
					Constants.ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLEE,
					Constants.ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLER);

			/*Final code cache difference should be higher than -1 and not equal to the initial code cache distance*/
			if ( finalCCDistance < 0 || (finalCCDistance == initialCCDistance)) {
				for ( int i = 0 ; i < 8 ; i++ ) {
					Thread.sleep(4000);
					finalCCDistance = TestManager.getLogParserDriver().getCodeCacheDifference(
							Constants.ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLEE,
							Constants.ADVANCED_TRAMPOLINE_TEST_METHOD_SIG_CALLER);
					if ( finalCCDistance > 0 && (finalCCDistance != initialCCDistance) ) {
						break;
					}
				}
			}

			System.out.println("Final code cache distance between callee() and caller1() = " + finalCCDistance);

			/*Validate that final code cache distance between callee() and caller1() is greater than 0, i.e., they are now in
			 * different code caches after callee() has been recompiled*/
			assertTrue ( finalCCDistance > 0 );

		} catch ( Exception e ) {
			e.printStackTrace();
			fail(e.getMessage());
		}
	}
}
