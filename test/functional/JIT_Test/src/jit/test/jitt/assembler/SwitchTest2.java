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
// Test file: SwitchTest2.java
// Testing the switch statement with different value types.

package jit.test.jitt.assembler;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class SwitchTest2 extends jit.test.jitt.Test {

	@Test
	public void testSwitchTest2() {
		int result=0;
		result=tstSwitch();

		if (result != 1)
			Assert.fail("tstSwitch failed!");
	}

	public int tstSwitch() {
		int success=1;

		//CASE1: Close Range of int case labels
		int test1 = 8;
		int check1 = 0;
		switch (test1) {
			case 1:  check1=1; break;
			case 2:  check1=2; break;
			case 3:  check1=3; break;
			case 4:  check1=4; break;
			case 5:  check1=5; break;
			case 6:  check1=6; break;
			case 7:  check1=7; break;
			case 8:  check1=8; break;
			case 9:  check1=9; break;
			case 10: check1=10; break;
			case 11: check1=11; break;
			case 12: check1=12; break;
		}

		if (check1 != test1)
			Assert.fail("SwitchTest2->run: Incorrect result for test #1!");

		//CASE2: Close Range of large int case labels
		int test2 = 2147483647;
		int check2 = 0;
		switch (test2) {
			case 2147483637:  check2=2147483637; break;
			case 2147483638:  check2=2147483638; break;
			case 2147483639:  check2=2147483639; break;
			case 2147483640:  check2=2147483640; break;
			case 2147483641:  check2=2147483641; break;
			case 2147483642:  check2=2147483642; break;
			case 2147483643:  check2=2147483643; break;
			case 2147483644:  check2=2147483644; break;
			case 2147483645:  check2=2147483645; break;
			case 2147483646:  check2=2147483646; break;
			case 2147483647:  check2=2147483647; break;
			default: check2=123456; break;
		}

		if (check2 != test2)
			Assert.fail("SwitchTest2->run: Incorrect result for test #2!");


		//CASE3: Close Range of large negative int case labels
		int test3 = -2147483648;
		int check3 = 0;
		switch (test3) {
			case -2147483640:  check3=-2147483640; break;
			case -2147483641:  check3=-2147483641; break;
			case -2147483642:  check3=-2147483642; break;
			case -2147483643:  check3=-2147483643; break;
			case -2147483644:  check3=-2147483644; break;
			case -2147483645:  check3=-2147483645; break;
			case -2147483646:  check3=-2147483646; break;
			case -2147483647:  check3=-2147483647; break;
			case -2147483648:  check3=-2147483648; break;
			default: check3=123456; break;
		}

		if (check3 != test3)
			Assert.fail("SwitchTest2->run: Incorrect result for test #3!");


		//CASE4: Close Range of numbers including neg, pos, zero
		int test4 = 0;
		int check4 = 0;
		switch (test4) {
			case -5:  check4=-5; break;
			case -4:  check4=-4; break;
			case -3:  check4=-3; break;
			case -2:  check4=-2; break;
			case -1:  check4=-1; break;
			case 0:  check4=0; break;
			case 1:  check4=1; break;
			case 2:  check4=2; break;
			case 3:  check4=3; break;
			default: check4=12345; break;
		}
		if (check4 != test4)
			Assert.fail("SwitchTest2->run: Incorrect result for test #4!");


		//TEST 5: Switch within a Switch
		//Only "inner" switch produces LDCTable Quad
		int test5 = 4;
		int check5 = 0;
		int subtest5=17;
		int subcheck5=0;

		switch (test5) {
			case 1:  check5=1; break;
			case 2:  check5=2; break;
			case 3:  check5=3; break;
			case 4:  {
				switch (subtest5) {
					case 11:  subcheck5=11; break;
					case 12:  subcheck5=12; break;
					case 13:  subcheck5=13; break;
					case 14:  subcheck5=14; break;
					case 15:  subcheck5=15; break;
					case 16:  subcheck5=16; break;
					case 17:  subcheck5=17; break;
					case 18:  subcheck5=18; break;
					case 19:  subcheck5=19; break;
					case 20:  subcheck5=20; break;
				}
				check5=4; break;
			}
			case 5:  check5=5; break;
			case 6:  check5=6; break;
			case 7:  check5=7; break;
			case 8:  check5=8; break;
			case 9:  check5=9; break;
			case 10:  check5=10; break;
			case 11:  check5=11; break;
			case 12:  check5=12; break;
			case 13: check5=13; break;
			case 14: check5=14; break;
			case 15: check5=15; break;
		}

		if (check5 != test5 || subcheck5 != subtest5)
			Assert.fail("SwitchTest2->run: Incorrect result for test #5!");


		return success;
	}
}
