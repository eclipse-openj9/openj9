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
public class TrampolineTest_IPIC extends TestCase {

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
	 * Interface polymorphic inline cache (PIC) dispatches requiring trampolines
	 */
	public void  testTrampolinePIC(){
		try {
			/*Ensure that the first implementer StringAdder gets loaded to an initial code cache.
			 * This triggers the first class resolution for IAdder.add(Object,Object). IPIC created for the test method will
			 * have IAddrImpl_String.add(String,String) at this point*/

			IAdder adder = new IAdderImpl_String();

			/*Since the caller to the interface methods must be compiled too, we create the caller instance first*/
			IAdderCaller iAdderCaller = new IAdderCaller();

			/*Call it twice so that the caller of the interface methods gets compiled as well*/
			Object result = iAdderCaller.callIAdder(adder, "a", "b");
			result = iAdderCaller.callIAdder(adder, "a", "b");

			//adder.add( "a", "b" );
			//Object result = adder.add( "a", "b" );

			assertEquals ( "ab", result );

			/*Sleep so that arbitrary methods get compiled and code caches get populated */

			int currentCodeCacheIndex = TestManager.getLogParserDriver().getCurrentCodeCacheIndex();
			while ( true ) {
				Thread.sleep(4000);
				if ( TestManager.getLogParserDriver().getCurrentCodeCacheIndex() > currentCodeCacheIndex + 10 ) {
					break;
				}
			}

			/*Load the second implementer of IAdder
			 *This time the runtime will look at the IPIC, and find out that the entry does not match, then this entry will be
			 *resolved and finally IAddrImpl_Int.add(int,int) will be added to the IPIC cache. The inter-codecache
			 *dispatching will happen implicitly.*/

			IAdder adder2 = new IAdderImpl_Int();
			//adder2.add( 1, 2 );
			//result = adder2.add( 2, 2 );

			result = iAdderCaller.callIAdder(adder2, 1, 2);
			result = iAdderCaller.callIAdder(adder2, 2, 2);

			assertEquals( 4, result );

			//*******
				//TODO:
				//Validate verbose message entry for IPIC usage once it becomes available.
				//The test is not considered complete until this is done.
			//*******

			/*Validate that the first implementer and second implementer are located in different code caches*/

			/*Wait a little bit so that the compilation info for IAdderImpl_Int.add() gets printed to log and read in by the LogParserThread */
			Thread.sleep(4000);

			int implemeterDistance = TestManager.getLogParserDriver().
					getCodeCacheDifference( Constants.IPIC_TRAMPOLINE_TEST_IADDERIMPL_INT,
							Constants.IPIC_TRAMPOLINE_TEST_IADDERIMPL_STR );

			if ( implemeterDistance < 0 ) {
				/*May be we did not wait long enough for the LogParserThread to update itself */
				for ( int i = 0 ; i < 24 ; i++ ){
					Thread.sleep(1000);
					implemeterDistance = TestManager.getLogParserDriver().
							getCodeCacheDifference( Constants.IPIC_TRAMPOLINE_TEST_IADDERIMPL_INT,
									Constants.IPIC_TRAMPOLINE_TEST_IADDERIMPL_STR );
					if ( implemeterDistance > -1 ) {
						break;
					}
				}
			}

			System.out.println("Code cache distance between IAdderImpl_Int.add() and IAdderImpl_String.add() = " + implemeterDistance);

			/*Validate the 2 implementers are in 2 different code caches*/
			assertTrue ( implemeterDistance > 0) ;

		} catch ( Exception e ) {
			e.printStackTrace();
			fail(e.getMessage());
		}
	}
}
