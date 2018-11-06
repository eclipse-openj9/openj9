/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity" }, invocationCount = 2)
public class Test_Math {
	long longValues[] = {0L, 1L, -1L, 42L, -42L, 88L, -88L, 0x80000000L, 0x7fffffffL, 0x8000000000000000L,
		0x7fffffff00000000L, 0x80000000ffffffffL, 0x7fffffffffffffffL, 0x7fffffff7fffffffL, Long.MAX_VALUE, Long.MIN_VALUE};

	int intValues[] = {0, 1, -1, 42, -42, 88, -88, 0x8000, 0x7fff, 0xffff8000,  Integer.MAX_VALUE, Integer.MIN_VALUE};

	/**
	* @tests java.lang.Math#Max()
	*/
	public void test_Max() {
		for (int i : intValues) {
			for (int j : intValues) {
				Assert.assertEquals(Math.max(i,j), i > j ? i : j, "Math.max failed on integers " + i + " and " + j);
			}
		}

		for (long i : longValues) {
			for (long j : longValues) {
				Assert.assertEquals(Math.max(i,j), i > j ? i : j, "Math.max failed on long values " + i + " and " + j);
			}
		}
	}

	/**
	* @tests java.lang.Math#Min()
	*/
	public void test_Min() {
		for (int i : intValues) {
			for (int j : intValues) {
				Assert.assertEquals(Math.min(i,j), i < j ? i : j, "Math.min failed on integers " + i + " and " + j);
			}
		}

		for (long i : longValues) {
			for (long j : longValues) {
				Assert.assertEquals(Math.min(i,j), i < j ? i : j, "Math.min failed on long values " + i + " and " + j);
			}
		}
	}
}
