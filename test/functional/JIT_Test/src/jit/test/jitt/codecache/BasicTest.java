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
 * Code Cache Junit test consisting of the basic test cases.
 * @author mesbah
 *
 */
public class BasicTest extends TestCase {

	/**
	 * Tests if the initial code caches have been created
	 */
	public void testIsCacheCreated() {

		boolean cacheCreated = false;
		LogParserThread logParserThread = TestManager.getLogParserDriver();

		for ( int i = 0 ; i < 60 ; i++ ) {
			if ( logParserThread.isCodeCacheCreated() == false ) {
				try {
					Thread.sleep(1000);
				} catch (InterruptedException e) {
						e.printStackTrace();
				}
			} else {
				cacheCreated = true;
				break;
			}
		}


		if( cacheCreated == false ) {
			System.out.println( "No Code Cache is created" );
		}
		else {
			System.out.println( "Test : Cache Creation : Passed" );
		}

		assertTrue( cacheCreated );
	}

	/**
	 * Tests if the first code cache is loading
	 */
	public void testIsCacheLoading() {

		boolean cacheLoading = false;
		LogParserThread logParserThread = TestManager.getLogParserDriver();

		for ( int i = 0 ; i < 120 ; i++ ) {

			if ( logParserThread.isCachePopulating() == false ) {
				try {
					Thread.sleep(1000);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			} else {
				cacheLoading = true;
				break;
			}
		}


		if( cacheLoading == false ) {
			System.out.println( "No Code cache is populated" );
		}
		else {
			System.out.println( "Test : Cache Population : Passed" );
		}

		assertTrue(cacheLoading);
	}

	/**
	 * Tests if method allocation has expanded from first code cache to the second code cache
	 */
	public void testIsCacheExpanding() {

		boolean cacheExpanding = false;
		LogParserThread logParserThread = TestManager.getLogParserDriver();

		for ( int i = 0 ; i < 240 ; i++ ) {
			if ( logParserThread.isCodeCacheExpanding() ) {
				cacheExpanding = true;
				break;
			} else {
				try {
					Thread.sleep(1000);
				}
				catch(InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
		if( cacheExpanding == false ) {
			System.out.println( "Code Cache not expanding" );
		}
		else {
			System.out.println( "Test : Cache Expansion : Passed" );
		}

		assertTrue ( cacheExpanding ) ;
	}
}
