/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
// Test file: SwitchTest.java
// Testing the switch statement with different value types. 

package jit.test.jitt.assembler;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class SwitchTest extends jit.test.jitt.Test {

	@Test
	public void testSwitchTest() {
		int result=0;
		result=tstSwitch();

		if (result != 1)
			Assert.fail("tstSwitch failed!");	
	}

	static int tstSwitch() {
		//CASE1: Large Range of int case labels
		int test1 = 5;
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
		}

		if (check1 != test1)
			Assert.fail("SwitchTest->run: Incorrect result for test #1A!");
	
		//CASE1B: Close Range of int case labels, but with T_LONG
		int test1b = 8;
		long check1b = 0L;
		switch (test1b) {
			case 1:  check1b=1L; break;
			case 2:  check1b=2L; break;
			case 3:  check1b=3L; break;
			case 4:  check1b=4L; break;
			case 5:  check1b=5L; break;
			case 6:  check1b=6L; break;
			case 7:  check1b=7L; break;
			case 8:  check1b=8L; break;
			case 9:  check1b=9L; break;
			case 10: check1b=10L; break;
			case 11: check1b=11L; break;
			case 12: check1b=12L; break;
		}

		if (check1b != test1b)
			Assert.fail("SwitchTest->run: Incorrect result for test #1B!");


		//CASE2: Choose Range of large int case labels

		int test2 = 2147483647;
		int check2 = 0;
		switch (test2) {
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
			Assert.fail("SwitchTest->run: Incorrect result for test #2!");

	
		//TEST3:  use char as the switch 
		char check3;
		char test3 = 'b';
		switch (test3) {
			case 	'a':  
			case   	'b':  check3='b'; break;
			
			case 	'c':  
			case	'd':  
			case	'e':  check3='e'; break;
			
			case 	'f':  
			case	'g':  check3='g'; break;
			default:
				check3='z'; break;
		}
		
		if (check3 != test3)
			Assert.fail("SwitchTest->run: Incorrect result for test #3!");


		//CASE4: Close Range of large negative int case labels
		int test4 = -2147483648;
		int check4 = 0;
		switch (test4) {
			case -2147483643:  check4=-2147483643; break;
			case -2147483644:  check4=-2147483644; break;
			case -2147483645:  check4=-2147483645; break;
			case -2147483646:  check4=-2147483646; break;
			case -2147483647:  check4=-2147483647; break;
			case -2147483648:  check4=-2147483648; break;
			default: check4=123456; break;
		}
		if (check4 != test4)
			Assert.fail("SwitchTest->run: Incorrect result for test #4!");


		//CASE 5: using byte
		byte test5 = 127;
		byte check5 = 0;
		switch (test5) {
			case -128:
			case -127:
			case -1:
				check5=-128; break;
			case 0:   check5=0; break;
			case 35:  check5=35; break;
			case 47:  check5=47; break;
			case 55:  check5=55; break;
			case 69:  check5=69; break;
			case 77:  check5=77; break;
			case 88:  check5=88; break;
			case 127: check5=127; break;
		}

		if (check5 != test5)
			Assert.fail("SwitchTest->run: Incorrect result for test #5!");

		//CASE 6: using short
		short test6 = -32768; //MIN VALUE of SHORT
		short check6 = 0;
		switch (test6) {
			case 32767:  check6=32767; break;
			case 0:  check6=0; break;
			case 6:  check6=6; break;
			case 47:  check6=47; break;
			case 366:  check6=366; break;
			case 969:  check6=969; break;
			case 2177:  check6=2177; break;
			case 7778:  check6=7788; break;
			case -32768: check6=-32768; break;
			default: check6=1234; break;
		}

		if (check6 != test6)
			Assert.fail("SwitchTest->run: Incorrect result for test #6!");


		//TEST 7: Switch within a Switch
		int test7 = 4;
		int check7 = 0;
		int subtest7=17;
		int subcheck7=0;
		
		switch (test7) {
			case 1:  check7=1; break;
			case 2:  check7=2; break;
			case 3:  check7=3; break;
			case 4:  {
				switch (subtest7) {
					case 11:  subcheck7=11; break;
					case 12:  subcheck7=12; break;
					case 13:  subcheck7=13; break;
					case 14:  subcheck7=14; break;
					case 15:  subcheck7=15; break;
					case 16:  subcheck7=16; break;
					case 17:  subcheck7=17; break;
					case 18:  subcheck7=18; break;
       			}
				check7=4; break;
			}
			case 5:  check7=5; break;
			case 6:  check7=6; break;
			case 7:  check7=7; break;
			case 8:  check7=8; break;
		}
		
		if (check7 != test7 || subcheck7 != subtest7)
			Assert.fail("SwitchTest->run: Incorrect result for test #7!");

		return 1;

	}
}
