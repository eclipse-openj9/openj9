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

public class SpaceReuseTest extends TestCase {

	/**
	 * Tests if after method reclamation, freed up space in previously filled code cache is reused
	 */
	public void testSpaceReuse() {

		boolean isSpaceReuseHappening = false;
		LogParserThread logParserThread = TestManager.getLogParserDriver();

		for ( int i = 0 ; i < 60 ; i++ ) {
			if ( logParserThread.isSpaceReuseHappening() ) {
				isSpaceReuseHappening = true;
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

		if( isSpaceReuseHappening == false ) {
			System.out.println( "No Space reclamation is happening in code caches" );
		}
		else {
			System.out.println( "Test : Code Cache Space Reclamation : Passed " );
		}

		assertTrue ( isSpaceReuseHappening ) ;
	}
}
