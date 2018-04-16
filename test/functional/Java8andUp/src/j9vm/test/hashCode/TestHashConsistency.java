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
package j9vm.test.hashCode;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;

@Test(groups = { "level.extended" })
public class TestHashConsistency {
	private static final Logger logger = Logger.getLogger(TestHashConsistency.class);
	/**
	 * Sanity test for hashcode consistency.
	 * Create an object and get its hashcode, then create a large number  of other objects to force
	 * a GC, and check that the original object still has the same hashcode.
	 */
	@Test(groups = { "level.extended" })
	public void testHashcodeDoesntChange() {
		EvenBiggerHashedObject testObject = new EvenBiggerHashedObject(0);
		int oldHashCode = testObject.hashCode();
		logger.debug("old hashcode " + oldHashCode);

		for (int i = 0; i < 1000000; ++i) {
			EvenBiggerHashedObject garbageObject = new EvenBiggerHashedObject(i);
			int newHash = testObject.hashCode();

			Assert.assertEquals(oldHashCode, newHash, "hashcode mismatch at " + i);
		}
	}
}
