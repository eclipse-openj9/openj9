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
package org.openj9.test.nestmates;

import java.util.Random;
import java.util.concurrent.ThreadLocalRandom;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

public class NestmatesTest {

	private static int LOOP_COUNT = 1000;

	private static long KEY1 = 0xdeadbeaf_AL;
	private static long KEY2 = 0xdeadbeaf_BL;
	private static long KEY3 = 0xdeadbeaf_CL;

	@Test(groups = { "level.sanity" })
	public void testVirtualUnresolvedInterpreted() {
		InnerClass ic = new InnerClass();
		int randomNum = ThreadLocalRandom.current().nextInt(0, 100);
		long ret = ic.innerPrivateMethod(randomNum % 3);

		checkReturnAndAssert(randomNum % 3, ret);
	}

	@Test(groups = { "level.sanity" })
	public void testVirtualUnresolvedJitted() {

		InnerClass ic = new InnerClass();
		int randomNum = ThreadLocalRandom.current().nextInt(0, 100);

		// innerPrivateMethod() should be interpreted
		long ret = ic.innerPrivateMethod(randomNum % 3);

		// call loopy method to trigger compilation with global disableAsyncCompilation
		loopyMethodInnerClassPrivate(ic);

		// the target should have been jit compiled by now.
		randomNum = ThreadLocalRandom.current().nextInt(0, 100);
		ret = ic.innerPrivateMethod(randomNum % 3);

		checkReturnAndAssert(randomNum % 3, ret);
	}

	// run this method with count=0. Also use rtResolve globally.
	@Test(groups = { "level.sanity" })
	public void testInterfaceUnresolvedInterpreted() {

		InnerInterface ii = new InnerClass();
		int randomNum = ThreadLocalRandom.current().nextInt(0, 100);

		long ret = ii.innerInterfacePrivateMethod(randomNum % 3);
		checkReturnAndAssert(randomNum % 3, ret);
	}

	// run this method with count=0. Also use rtResolve globally
	@Test(groups = { "level.sanity" })
	public void testInterfaceUnresolvedJitted() {
		InnerInterface ii = new InnerClass();
		int randomNum = ThreadLocalRandom.current().nextInt(0, 100);

		// the target is not jit compiled yet
		long ret = ii.innerInterfacePrivateMethod(randomNum % 3);

		// call loopy method to trigger the compilation. make sure
		// that innerInterfacePrivateMethod() gets a count=100
		loopyMethodInterfacePrivate(ii);

		// this should have been jit compiled. The
		randomNum = ThreadLocalRandom.current().nextInt(0, 100);
		ret = ii.innerInterfacePrivateMethod(randomNum % 3);
		checkReturnAndAssert(randomNum % 3, ret);
	}

	private void checkReturnAndAssert(int i, long l) {
		if ((i == 0) && (l != NestmatesTest.KEY1) || (i == 1) && (l != NestmatesTest.KEY2)
				|| (i == 2) && (l != NestmatesTest.KEY3)) {
			Assert.fail("Invalid return for " + i + " " + l);
		}

		return;
	}

	/**
	 * \brief Invoke the target enough times to trigger JIT compilation
	 */
	private void loopyMethodInterfacePrivate(InnerInterface ii) {
		long ret = 0;
		int randomNum = ThreadLocalRandom.current().nextInt(0, 100);

		for (int i = 0; i < LOOP_COUNT; ++i)
			ret = ii.innerInterfacePrivateMethod(randomNum % 3);
	}

	/**
	 * \brief Invoke the target enough times to trigger JIT compilation
	 */
	private void loopyMethodInnerClassPrivate(InnerClass ic) {
		for (int i = 0; i < LOOP_COUNT; ++i) {
			ic.innerPrivateMethod(i % 3);
		}
	}

	interface InnerInterface {

		public void interface_call1();

		private long innerInterfacePrivateMethod(int i) {
			long retVal = 0;

			switch (i) {
			case 0:
				retVal = NestmatesTest.KEY1;
				break;
			case 1:
				retVal = NestmatesTest.KEY2;
				break;
			default:
				retVal = NestmatesTest.KEY3;
				break;
			}

			return retVal;
		}
	}

	public class InnerClass implements InnerInterface {

		@Override
		public void interface_call1() {
			System.out.println("Not implemented");
		}

		// This innerPrivateMethod() should have a count=100
		private long innerPrivateMethod(int i) {
			long retVal = 0;

			switch (i) {
			case 0:
				retVal = NestmatesTest.KEY1;
				break;
			case 1:
				retVal = NestmatesTest.KEY2;
				break;
			default:
				retVal = NestmatesTest.KEY3;
				break;
			}

			return retVal;
		}

	}
}
