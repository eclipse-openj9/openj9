/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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

@Test(groups = { "level.sanity" })
public class TestCompareAndSwap extends UnsafeTestBase{
	private static Logger logger = Logger.getLogger(TestCompareAndSwap.class);

	public TestCompareAndSwap(String scenario) {
		super(scenario);
	}
	
	/* get logger to use, for child classes to report with their class name instead of UnsafeTestBase*/
	protected Logger getLogger() {
		return logger;
	}
	
	@Override
	@BeforeMethod
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance2();
	}

	public void testInstanceCompareAndSwapInt() throws Exception {
		testInt(new IntData(), COMPAREANDSWAP);
	}
	
	public void testInstanceCompareAndSwapLong() throws Exception {
		testLong(new LongData(), COMPAREANDSWAP);
	}

	public void testArrayCompareAndSwapInt() throws Exception {
		testInt(new int[modelInt.length], COMPAREANDSWAP);
	}
	
	public void testArrayCompareAndSwapLong() throws Exception {
		testLong(new long[modelLong.length], COMPAREANDSWAP);
	}

	public void testStaticCompareAndSwapInt() throws Exception {
		testInt(IntData.class, COMPAREANDSWAP);
	}
	
	public void testStaticCompareAndSwapLong() throws Exception {
		testLong(LongData.class, COMPAREANDSWAP);
	}
	
	public void testObjectNullCompareAndSwapInt() throws Exception {
		memAllocate(100);
		alignment();
		testIntNative(COMPAREANDSWAP);		
	}
	
	public void testObjectNullCompareAndSwapLong() throws Exception {
		memAllocate(100);
		alignment();
		testLongNative(COMPAREANDSWAP);
	}
}
