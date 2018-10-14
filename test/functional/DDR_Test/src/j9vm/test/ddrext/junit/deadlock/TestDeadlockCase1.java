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
package j9vm.test.ddrext.junit.deadlock;

import j9vm.test.ddrext.Constants;
import j9vm.test.ddrext.DDRExtTesterBase;

public class TestDeadlockCase1 extends DDRExtTesterBase
{
	/**
	 * Test !monitors deadlock
	 *      Test Case #1
	 *	  JOO, JOO deadlock.
	 */
	
	public void testDeadlock1()
	{
		String output = exec(Constants.MONITORS_CMD, new String[] { Constants.DEADLOCK_CMD });
		
		if (null == output) {
			fail("\"!monitors deadlock\" output is null. Can not proceed with test");
			return;
		}
		
		assertTrue(validate(output, Constants.DEADLOCK_THREAD, 3));
		assertTrue(validate(output, Constants.DEADLOCK_BLOCKING_ON, 2));
		assertTrue(validate(output, Constants.DEADLOCK_OWNED_BY, 2));
		assertTrue(validate(output, Constants.DEADLOCK_JAVA_OBJ, 2));
	}

}
