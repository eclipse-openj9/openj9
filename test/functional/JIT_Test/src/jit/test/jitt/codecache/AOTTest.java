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
 * This Junit test case implements the second phase of the code cache AOT test
 * @author mesbah
 *
 */
public class AOTTest extends TestCase{

	protected void setUp() throws Exception {
		super.setUp();

		/*We need to start the log parser thread so that we can gather
		 * AOT compilation information required for this test*/

		TestManager.startLogParserInBackground( Constants.AOT_TEST );
	}

	@Override
	protected void tearDown() throws Exception {
		super.tearDown();

		/*Shut down the log parser thread as the tear down process of this test*/

		boolean logParserStopped = TestManager.stopLogParser();
		assertTrue(logParserStopped);
	}

	/**
	 * Code Cache AOT test : In this test we invoke a target method which should be AOT'ed in the first phase of the AOT test.
	 * For the first phase of AOT test, please see TestManager.loadAot() method.
	*/
	public void testAOT() {

		/*First verify if -Xhareclasses option was specified in VM arguments*/

		/*VM options required for this test :
		 * -Xjit:count=1,code=1024,numCodeCachesOnStartup=1,verbose={"codecache|compileEnd"},
		 * vlog=<logdir>/<name>.log -Xshareclasses:name=<cachename used in phase 1>
		 * */
		if ( !TestManager.isShareClassesSet() ) {
			fail("Please speficy -Xshareclasses:<cachename> in VM options, where <cachename> is the name of the " +
					"shared class cache used in phase 1 of AOT test");
		}

		/*Invoke the AOT-loaded target using a caller that is compiled and placed in shared class cache*/

		Caller2_AotTest caller2 = new Caller2_AotTest();
		int result = caller2.caller();
		result = caller2.caller();

		/*Validate that target is still returning correct result*/
		assertEquals(343000,result);

		/*Validate that the target was aot-loaded.*/

		boolean isTargetAotLoaded = false;

		for ( int i = 0 ; i < 10 ; i++ ){

			/*Check if the target method is present in the list of AOT'ed methods
			 * the LogParserThread has built up so far*/
			isTargetAotLoaded = TestManager.getLogParserDriver().isAotLoaded( Constants.AOT_TEST_TARGET_METHOD_SIG );

			if ( isTargetAotLoaded ) {
				break;
			}

			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}

		if ( !isTargetAotLoaded ) {
			System.out.println("The target method was not AOT loaded : " + Constants.AOT_TEST_TARGET_METHOD_SIG );
		}

		assertTrue( isTargetAotLoaded ) ;
	}
}
