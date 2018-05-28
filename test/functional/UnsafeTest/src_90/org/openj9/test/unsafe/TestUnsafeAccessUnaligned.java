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
package org.openj9.test.unsafe;

import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/* note: size of test array is one more than model so we can put and
 * get memory unaligned outside the model array without corrupting 
 * memory.
 */
@Test(groups = { "level.sanity" })
public class TestUnsafeAccessUnaligned extends UnsafeTestBase {
	private static Logger logger = Logger.getLogger(TestUnsafeAccessUnaligned.class);

	public TestUnsafeAccessUnaligned(String scenario) {
		super(scenario);
	}

	/*
	 * get logger to use, for child classes to report with their class name instead
	 * of UnsafeTestBase
	 */
	@Override
	protected Logger getLogger() {
		return logger;
	}

	@Override
	@BeforeMethod
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance2();
	}

	// tests for testArrayPutXXXXUnaligned
	public void testArrayPutCharUnaligned() throws Exception {
		testChar(new char[modelChar.length + 1], UNALIGNED);
	}

	public void testArrayPutShortUnaligned() throws Exception {
		testShort(new short[modelShort.length + 1], UNALIGNED);
	}

	public void testArrayPutIntUnaligned() throws Exception {
		testInt(new int[modelInt.length + 1], UNALIGNED);
	}

	public void testArrayPutLongUnaligned() throws Exception {
		testLong(new long[modelLong.length + 1], UNALIGNED);
	}

	// tests for testObjectNullPutXXXXUnaligned
	public void testObjectNullPutCharUnaligned() throws Exception {
		testCharNative(UNALIGNED);
	}

	public void testObjectNullPutShortUnaligned() throws Exception {
		testShortNative(UNALIGNED);
	}

	public void testObjectNullPutIntUnaligned() throws Exception {
		testIntNative(UNALIGNED);
	}

	public void testObjectNullPutLongUnaligned() throws Exception {
		testLongNative(UNALIGNED);
	}

	// tests for testArrayGetXXXXUnaligned
	public void testArrayGetCharUnaligned() throws Exception {
		testGetChar(new char[modelChar.length + 1], UNALIGNED);
	}

	public void testArrayGetShortUnaligned() throws Exception {
		testGetShort(new short[modelShort.length + 1], UNALIGNED);
	}

	public void testArrayGetIntUnaligned() throws Exception {
		testGetInt(new int[modelInt.length + 1], UNALIGNED);
	}

	public void testArrayGetLongUnaligned() throws Exception {
		testGetLong(new long[modelLong.length + 1], UNALIGNED);
	}
}
