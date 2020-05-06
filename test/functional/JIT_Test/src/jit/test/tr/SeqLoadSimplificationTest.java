/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
package jit.test.tr;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class SeqLoadSimplificationTest {
	private final byte[] byteArray;
	private final int[]  dummyIntArray;
	private final long[] dummyLongArray;

	SeqLoadSimplificationTest() {
		byteArray = new byte[16];
		dummyLongArray = new long[16];
		dummyIntArray = new int[16];
		for (int i = 0; i < byteArray.length; i++) {
			byteArray[i] = (byte)(i + 1 + 0x80);
		}
	}

	/**
	 * These tests exercise the sequentialStoreSimplification opt.
	 *
	 * The sequentialStoreSimplification opt, despite the name, also optimizes byte loads to consecutive
	 * memory addresses where the loaded bytes are used to construct a longer value such as an int or long. If a
	 * pattern match is found, the byte loads are replace with a smaller number of wider loads such as an int load
	 * or long load.
	 *
	 * These tests test various patterns of loading bytes. Some of which are expected the trigger the opt and some
	 * are patterns where the opt is supposed to bail out.
	 */
	@Test
	public void testSeqLoadLoop() {
		SeqLoadSimplificationTest t = new SeqLoadSimplificationTest();

		for (int i = 0; i < 100; i++) {
			t.runTestCombineLE01(0);
			t.runTestCombineLE02(0);
			t.runTestCombineLE03(0);
			t.runTestCombineLE04(0);
			t.runTestCombineLE05(0);
			t.runTestCombineLE06(0);
			t.runTestCombineLE07(0);
			t.runTestCombineLE08(0);
			t.runTestCombineLE09(0);
			t.runTestCombineLE10(0);
			t.runTestCombineLE11(0);
			t.runTestCombineLE12(4);
			t.runTestCombineLE13(0);
			t.runTestCombineLE14(0);
			t.runTestCombineLE15(0);
			t.runTestCombineLE16(0);
			t.runTestCombineLE17(0);
			t.runTestCombineLE18(0);
			t.runTestCombineLE19(0);
			t.runTestCombineLE20(0);
			t.runTestCombineLE21(0);
			t.runTestCombineLE22(0);
			t.runTestCombineLE23(0);
			t.runTestCombineLE24(0);
			t.runTestCombineLE25(0);
			t.runTestCombineLE26(0);
			t.runTestCombineLE27(0);
			t.runTestCombineLE28(0);
			t.runTestCombineLE29(0);
			t.runTestCombineLE30(0);
			t.runTestCombineLE31(0);

			t.runTestCombineBE01(0);
			t.runTestCombineBE02(0);
			t.runTestCombineBE03(0);
			t.runTestCombineBE04(0);
			t.runTestCombineBE05(0);
			t.runTestCombineBE06(0);
			t.runTestCombineBE07(0);
			t.runTestCombineBE08(0);
			t.runTestCombineBE09(0);
			t.runTestCombineBE10(0);
			t.runTestCombineBE11(0);
			t.runTestCombineBE12(4);
			t.runTestCombineBE13(0);
			t.runTestCombineBE14(0);
			t.runTestCombineBE15(0);
			t.runTestCombineBE16(0);
			t.runTestCombineBE17(0);
			t.runTestCombineBE18(0);
			t.runTestCombineBE19(0);
			t.runTestCombineBE20(0);
			t.runTestCombineBE21(0);
			t.runTestCombineBE22(0);
			t.runTestCombineBE23(0);
			t.runTestCombineBE24(0);
			t.runTestCombineBE25(0);
			t.runTestCombineBE26(0);
			t.runTestCombineBE27(0);
			t.runTestCombineBE28(0);
			t.runTestCombineBE29(0);
			t.runTestCombineBE30(0);
			t.runTestCombineBE31(0);

			t.runTestSplitLE01(0);
			t.runTestSplitLE02(0);
			t.runTestSplitLE03(0);
			t.runTestSplitLE04(0);
			t.runTestSplitLE05(0);
			t.runTestSplitLE06(0);
			t.runTestSplitLE07(0);
			t.runTestSplitLE08(0);
			t.runTestSplitLE09(0);

			t.runTestSplitBE01(0);
			t.runTestSplitBE02(0);
			t.runTestSplitBE03(0);
			t.runTestSplitBE04(0);
			t.runTestSplitBE05(0);
			t.runTestSplitBE06(0);
			t.runTestSplitBE07(0);
			t.runTestSplitBE08(0);
			t.runTestSplitBE09(0);
		}

		System.out.println("Verifying runTestCombineLE tests.");
		AssertJUnit.assertEquals("runTestCombineLE01 returned an incorrect result.", t.runTestCombineLE01(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE02 returned an incorrect result.", t.runTestCombineLE02(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE03 returned an incorrect result.", t.runTestCombineLE03(0), 0xffffffff88878685L);
		AssertJUnit.assertEquals("runTestCombineLE04 returned an incorrect result.", t.runTestCombineLE04(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE05 returned an incorrect result.", t.runTestCombineLE05(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE06 returned an incorrect result.", t.runTestCombineLE06(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE07 returned an incorrect result.", t.runTestCombineLE07(0), 0x8281L);
		AssertJUnit.assertEquals("runTestCombineLE08 returned an incorrect result.", t.runTestCombineLE08(0), 0x838281L);
		AssertJUnit.assertEquals("runTestCombineLE09 returned an incorrect result.", t.runTestCombineLE09(0), 0x8887868584838281L);
		AssertJUnit.assertEquals("runTestCombineLE10 returned an incorrect result.", t.runTestCombineLE10(0), 0x8887868584838281L);
		AssertJUnit.assertEquals("runTestCombineLE11 returned an incorrect result.", t.runTestCombineLE11(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE12 returned an incorrect result.", t.runTestCombineLE12(4), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE13 returned an incorrect result.", t.runTestCombineLE13(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE14 returned an incorrect result.", t.runTestCombineLE14(0), 0x84838281L);
		AssertJUnit.assertEquals("runTestCombineLE15 returned an incorrect result.", t.runTestCombineLE15(0), 0x838281L);
		AssertJUnit.assertEquals("runTestCombineLE16 returned an incorrect result.", t.runTestCombineLE16(0), 0x8281L);
		AssertJUnit.assertEquals("runTestCombineLE17 returned an incorrect result.", t.runTestCombineLE17(0), 0xffffffffffff8281L);
		AssertJUnit.assertEquals("runTestCombineLE18 returned an incorrect result.", t.runTestCombineLE18(0), 0xffffffffff838281L);
		AssertJUnit.assertEquals("runTestCombineLE19 returned an incorrect result.", t.runTestCombineLE19(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE20 returned an incorrect result.", t.runTestCombineLE20(0), 0xffffffffff838281L);
		AssertJUnit.assertEquals("runTestCombineLE21 returned an incorrect result.", t.runTestCombineLE21(0), 0xffffffffffff8281L);
		AssertJUnit.assertEquals("runTestCombineLE22 returned an incorrect result.", t.runTestCombineLE22(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestCombineLE23 returned an incorrect result.", t.runTestCombineLE23(0), 0x8584838281L);
		AssertJUnit.assertEquals("runTestCombineLE24 returned an incorrect result.", t.runTestCombineLE24(0), 0xffffff8584838281L);
		AssertJUnit.assertEquals("runTestCombineLE25 returned an incorrect result.", t.runTestCombineLE25(0), 0x868584838281L);
		AssertJUnit.assertEquals("runTestCombineLE26 returned an incorrect result.", t.runTestCombineLE26(0), 0xffff868584838281L);
		AssertJUnit.assertEquals("runTestCombineLE27 returned an incorrect result.", t.runTestCombineLE27(0), 0x87868584838281L);
		AssertJUnit.assertEquals("runTestCombineLE28 returned an incorrect result.", t.runTestCombineLE28(0), 0xff87868584838281L);
		AssertJUnit.assertEquals("runTestCombineLE29 returned an incorrect result.", t.runTestCombineLE29(0), 0x8b8a898887868584L);
		AssertJUnit.assertEquals("runTestCombineLE30 returned an incorrect result.", t.runTestCombineLE30(0), 0x8b8a898887868584L);
		AssertJUnit.assertEquals("runTestCombineLE31 returned an incorrect result.", t.runTestCombineLE31(0), 0x908f8e8d8c8b8a89L);

		System.out.println("Verifying runTestCombineBE tests.");
		AssertJUnit.assertEquals("runTestCombineBE01 returned an incorrect result.", t.runTestCombineBE01(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE02 returned an incorrect result.", t.runTestCombineBE02(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE03 returned an incorrect result.", t.runTestCombineBE03(0), 0xffffffff85868788L);
		AssertJUnit.assertEquals("runTestCombineBE04 returned an incorrect result.", t.runTestCombineBE04(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE05 returned an incorrect result.", t.runTestCombineBE05(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE06 returned an incorrect result.", t.runTestCombineBE06(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE07 returned an incorrect result.", t.runTestCombineBE07(0), 0x8182L);
		AssertJUnit.assertEquals("runTestCombineBE08 returned an incorrect result.", t.runTestCombineBE08(0), 0x818283L);
		AssertJUnit.assertEquals("runTestCombineBE09 returned an incorrect result.", t.runTestCombineBE09(0), 0x8182838485868788L);
		AssertJUnit.assertEquals("runTestCombineBE10 returned an incorrect result.", t.runTestCombineBE10(0), 0x8182838485868788L);
		AssertJUnit.assertEquals("runTestCombineBE11 returned an incorrect result.", t.runTestCombineBE11(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE12 returned an incorrect result.", t.runTestCombineBE12(4), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE13 returned an incorrect result.", t.runTestCombineBE13(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE14 returned an incorrect result.", t.runTestCombineBE14(0), 0x81828384L);
		AssertJUnit.assertEquals("runTestCombineBE15 returned an incorrect result.", t.runTestCombineBE15(0), 0x818283L);
		AssertJUnit.assertEquals("runTestCombineBE16 returned an incorrect result.", t.runTestCombineBE16(0), 0x8182L);
		AssertJUnit.assertEquals("runTestCombineBE17 returned an incorrect result.", t.runTestCombineBE17(0), 0xffffffffffff8182L);
		AssertJUnit.assertEquals("runTestCombineBE18 returned an incorrect result.", t.runTestCombineBE18(0), 0xffffffffff818283L);
		AssertJUnit.assertEquals("runTestCombineBE19 returned an incorrect result.", t.runTestCombineBE19(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE20 returned an incorrect result.", t.runTestCombineBE20(0), 0xffffffffff818283L);
		AssertJUnit.assertEquals("runTestCombineBE21 returned an incorrect result.", t.runTestCombineBE21(0), 0xffffffffffff8182L);
		AssertJUnit.assertEquals("runTestCombineBE22 returned an incorrect result.", t.runTestCombineBE22(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestCombineBE23 returned an incorrect result.", t.runTestCombineBE23(0), 0x8182838485L);
		AssertJUnit.assertEquals("runTestCombineBE24 returned an incorrect result.", t.runTestCombineBE24(0), 0xffffff8182838485L);
		AssertJUnit.assertEquals("runTestCombineBE25 returned an incorrect result.", t.runTestCombineBE25(0), 0x818283848586L);
		AssertJUnit.assertEquals("runTestCombineBE26 returned an incorrect result.", t.runTestCombineBE26(0), 0xffff818283848586L);
		AssertJUnit.assertEquals("runTestCombineBE27 returned an incorrect result.", t.runTestCombineBE27(0), 0x81828384858687L);
		AssertJUnit.assertEquals("runTestCombineBE28 returned an incorrect result.", t.runTestCombineBE28(0), 0xff81828384858687L);
		AssertJUnit.assertEquals("runTestCombineBE29 returned an incorrect result.", t.runTestCombineBE29(0), 0x8485868788898a8bL);
		AssertJUnit.assertEquals("runTestCombineBE30 returned an incorrect result.", t.runTestCombineBE30(0), 0x8485868788898a8bL);
		AssertJUnit.assertEquals("runTestCombineBE31 returned an incorrect result.", t.runTestCombineBE31(0), 0x898a8b8c8d8e8f90L);

		System.out.println("Verifying runTestSplitLE tests.");
		AssertJUnit.assertEquals("runTestSplitLE01 returned an incorrect result.", t.runTestSplitLE01(0), 0xffffffff84838181L);
		AssertJUnit.assertEquals("runTestSplitLE02 returned an incorrect result.", t.runTestSplitLE02(0), 0xffffffff85838281L);
		AssertJUnit.assertEquals("runTestSplitLE03 returned an incorrect result.", t.runTestSplitLE03(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestSplitLE04 returned an incorrect result.", t.runTestSplitLE04(0), 0x8010581L);
		AssertJUnit.assertEquals("runTestSplitLE05 returned an incorrect result.", t.runTestSplitLE05(0), 0xffffffff82000081L);
		AssertJUnit.assertEquals("runTestSplitLE06 returned an incorrect result.", t.runTestSplitLE06(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestSplitLE07 returned an incorrect result.", t.runTestSplitLE07(0), 0xffffffff84838303L);
		AssertJUnit.assertEquals("runTestSplitLE08 returned an incorrect result.", t.runTestSplitLE08(0), 0xffffffff84838280L);
		AssertJUnit.assertEquals("runTestSplitLE09 returned an incorrect result.", t.runTestSplitLE09(0), 0x828100L);

		System.out.println("Verifying runTestSplitBE tests.");
		AssertJUnit.assertEquals("runTestSplitBE01 returned an incorrect result.", t.runTestSplitBE01(0), 0xffffffff81828284L);
		AssertJUnit.assertEquals("runTestSplitBE02 returned an incorrect result.", t.runTestSplitBE02(0), 0xffffffff81828385L);
		AssertJUnit.assertEquals("runTestSplitBE03 returned an incorrect result.", t.runTestSplitBE03(0), 0xffffffff84838281L);
		AssertJUnit.assertEquals("runTestSplitBE04 returned an incorrect result.", t.runTestSplitBE04(0), 0x2010584L);
		AssertJUnit.assertEquals("runTestSplitBE05 returned an incorrect result.", t.runTestSplitBE05(0), 0xffffffff81000082L);
		AssertJUnit.assertEquals("runTestSplitBE06 returned an incorrect result.", t.runTestSplitBE06(0), 0xffffffff81828384L);
		AssertJUnit.assertEquals("runTestSplitBE07 returned an incorrect result.", t.runTestSplitBE07(0), 0xffffffff81828407L);
		AssertJUnit.assertEquals("runTestSplitBE08 returned an incorrect result.", t.runTestSplitBE08(0), 0xffffffff81828380L);
		AssertJUnit.assertEquals("runTestSplitBE09 returned an incorrect result.", t.runTestSplitBE09(0), 0x818200L);
	}

	private int runTestCombineLE01(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) << 16) +
				  ((byteArray[i+3] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineLE02(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) << 16) +
				  ((byteArray[i+3]       ) << 24);
		return returnValue;
	}

	private int runTestCombineLE03(int i) {
		int returnValue = ((byteArray[i+4] & 0xFF)      ) +
				  ((byteArray[i+5] & 0xFF) <<  8) +
				  ((byteArray[i+6] & 0xFF) << 16) +
				  ((byteArray[i+7] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineLE04(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) |
				  ((byteArray[i+1] & 0xFF) <<  8) |
				  ((byteArray[i+2] & 0xFF) << 16) |
				  ((byteArray[i+3] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineLE05(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)            ) +
				  ((byteArray[i+1] & 0xFF) * 0x100    ) +
				  ((byteArray[i+2] & 0xFF) * 0x10000  ) +
				  ((byteArray[i+3] & 0xFF) * 0x1000000);
		return returnValue;
	}

	private int runTestCombineLE06(int i) {
		int a = ((byteArray[i+0] & 0xFF)      );
		int b = ((byteArray[i+1] & 0xFF) <<  8);
		int c = ((byteArray[i+2] & 0xFF) << 16);
		int d = ((byteArray[i+3] & 0xFF) << 24);

		int returnValue = a + b + c + d;

		return returnValue;
	}

	private int runTestCombineLE07(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8);
		return returnValue;
	}

	private int runTestCombineLE08(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) << 16);
		return returnValue;
	}

	private long runTestCombineLE09(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((byteArray[i+4] & 0xFFL) << 32) +
				   ((byteArray[i+5] & 0xFFL) << 40) +
				   ((byteArray[i+6] & 0xFFL) << 48) +
				   ((byteArray[i+7] & 0xFFL) << 56);
		return returnValue;
	}

	private long runTestCombineLE10(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)                     ) |
				   ((byteArray[i+1] & 0xFFL) * 0x100L            ) |
				   ((byteArray[i+2] & 0xFFL) * 0x10000L          ) |
				   ((byteArray[i+3] & 0xFFL) * 0x1000000L        ) |
				   ((byteArray[i+4] & 0xFFL) * 0x100000000L      ) |
				   ((byteArray[i+5] & 0xFFL) * 0x10000000000L    ) |
				   ((byteArray[i+6] & 0xFFL) * 0x1000000000000L  ) |
				   ((byteArray[i+7] & 0xFFL) * 0x100000000000000L);
		return returnValue;
	}

	private int runTestCombineLE11(int i) {
		int returnValue = ((byteArray[0] & 0xFF)      ) +
				  ((byteArray[1] & 0xFF) <<  8) +
				  ((byteArray[2] & 0xFF) << 16) +
				  ((byteArray[3] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineLE12(int i) {
		int returnValue = ((byteArray[i-4] & 0xFF)      ) +
				  ((byteArray[i-3] & 0xFF) <<  8) +
				  ((byteArray[i-2] & 0xFF) << 16) +
				  ((byteArray[i-1] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineLE13(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) << 16) +
				  ((byteArray[i+3]       ) << 24);

		dummyIntArray[0] = returnValue;
		return returnValue;
	}

	private long runTestCombineLE14(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24);

		dummyLongArray[0] = returnValue;
		return returnValue;
	}

	private long runTestCombineLE15(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16);

		return returnValue;
	}

	private long runTestCombineLE16(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8);

		return returnValue;
	}

	private int runTestCombineLE17(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1]       ) <<  8);
		return returnValue;
	}

	private int runTestCombineLE18(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2]       ) << 16);
		return returnValue;
	}

	private long runTestCombineLE19(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((long)(byteArray[i+3]  ) << 24);

		return returnValue;
	}

	private long runTestCombineLE20(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((long)(byteArray[i+2]  ) << 16);

		return returnValue;
	}

	private long runTestCombineLE21(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((long)(byteArray[i+1]  ) <<  8);

		return returnValue;
	}

	private int runTestCombineLE22(int i) {
		int returnValue = (((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8)) +
				  (((byteArray[i+2] & 0xFF) << 16) +
				  ((byteArray[i+3] & 0xFF) << 24));
		return returnValue;
	}

	private long runTestCombineLE23(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((byteArray[i+4] & 0xFFL) << 32);
		return returnValue;
	}

	private long runTestCombineLE24(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((long)(byteArray[i+4]  ) << 32);
		return returnValue;
	}

	private long runTestCombineLE25(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((byteArray[i+4] & 0xFFL) << 32) +
				   ((byteArray[i+5] & 0xFFL) << 40);
		return returnValue;
	}

	private long runTestCombineLE26(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((byteArray[i+4] & 0xFFL) << 32) +
				   ((long)(byteArray[i+5]  ) << 40);
		return returnValue;
	}

	private long runTestCombineLE27(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((byteArray[i+4] & 0xFFL) << 32) +
				   ((byteArray[i+5] & 0xFFL) << 40) +
				   ((byteArray[i+6] & 0xFFL) << 48);
		return returnValue;
	}

	private long runTestCombineLE28(int i) {
		long returnValue = ((byteArray[i+0] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((byteArray[i+4] & 0xFFL) << 32) +
				   ((byteArray[i+5] & 0xFFL) << 40) +
				   ((long)(byteArray[i+6]  ) << 48);
		return returnValue;
	}

	private long runTestCombineLE29(int i) {
		long returnValue = ((byteArray[i+3] & 0xFFL)      ) +
				   ((byteArray[i+4] & 0xFFL) <<  8) +
				   ((byteArray[i+5] & 0xFFL) << 16) +
				   ((byteArray[i+6] & 0xFFL) << 24) +
				   ((byteArray[i+7] & 0xFFL) << 32) +
				   ((byteArray[i+8] & 0xFFL) << 40) +
				   ((byteArray[i+9] & 0xFFL) << 48) +
				   ((byteArray[i+10] & 0xFFL) << 56);
		return returnValue;
	}

	private long runTestCombineLE30(int i) {
		long returnValue = ((byteArray[3] & 0xFFL)      ) +
				   ((byteArray[4] & 0xFFL) <<  8) +
				   ((byteArray[5] & 0xFFL) << 16) +
				   ((byteArray[6] & 0xFFL) << 24) +
				   ((byteArray[7] & 0xFFL) << 32) +
				   ((byteArray[8] & 0xFFL) << 40) +
				   ((byteArray[9] & 0xFFL) << 48) +
				   ((byteArray[10] & 0xFFL) << 56);
		return returnValue;
	}

	private long runTestCombineLE31(int i) {
		long returnValue = ((byteArray[8] & 0xFFL)      ) +
				   ((byteArray[9] & 0xFFL) <<  8) +
				   ((byteArray[10] & 0xFFL) << 16) +
				   ((byteArray[11] & 0xFFL) << 24) +
				   ((byteArray[12] & 0xFFL) << 32) +
				   ((byteArray[13] & 0xFFL) << 40) +
				   ((byteArray[14] & 0xFFL) << 48) +
				   ((byteArray[15] & 0xFFL) << 56);
		return returnValue;
	}

	private int runTestCombineBE01(int i) {
		int returnValue = ((byteArray[i+3] & 0xFF)      ) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) << 16) +
				  ((byteArray[i+0] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineBE02(int i) {
		int returnValue = ((byteArray[i+3] & 0xFF)      ) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) << 16) +
				  ((byteArray[i+0]       ) << 24);
		return returnValue;
	}

	private int runTestCombineBE03(int i) {
		int returnValue = ((byteArray[i+7] & 0xFF)      ) +
				  ((byteArray[i+6] & 0xFF) <<  8) +
				  ((byteArray[i+5] & 0xFF) << 16) +
				  ((byteArray[i+4] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineBE04(int i) {
		int returnValue = ((byteArray[i+3] & 0xFF)      ) |
				  ((byteArray[i+2] & 0xFF) <<  8) |
				  ((byteArray[i+1] & 0xFF) << 16) |
				  ((byteArray[i+0] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineBE05(int i) {
		int returnValue = ((byteArray[i+3] & 0xFF)            ) +
				  ((byteArray[i+2] & 0xFF) * 0x100    ) +
				  ((byteArray[i+1] & 0xFF) * 0x10000  ) +
				  ((byteArray[i+0] & 0xFF) * 0x1000000);
		return returnValue;
	}

	private int runTestCombineBE06(int i) {
		int a = ((byteArray[i+3] & 0xFF)      );
		int b = ((byteArray[i+2] & 0xFF) <<  8);
		int c = ((byteArray[i+1] & 0xFF) << 16);
		int d = ((byteArray[i+0] & 0xFF) << 24);

		int returnValue = a + b + c + d;

		return returnValue;
	}

	private int runTestCombineBE07(int i) {
		int returnValue = ((byteArray[i+1] & 0xFF)      ) +
				  ((byteArray[i+0] & 0xFF) <<  8);
		return returnValue;
	}

	private int runTestCombineBE08(int i) {
		int returnValue = ((byteArray[i+2] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+0] & 0xFF) << 16);
		return returnValue;
	}

	private long runTestCombineBE09(int i) {
		long returnValue = ((byteArray[i+7] & 0xFFL)      ) +
				   ((byteArray[i+6] & 0xFFL) <<  8) +
				   ((byteArray[i+5] & 0xFFL) << 16) +
				   ((byteArray[i+4] & 0xFFL) << 24) +
				   ((byteArray[i+3] & 0xFFL) << 32) +
				   ((byteArray[i+2] & 0xFFL) << 40) +
				   ((byteArray[i+1] & 0xFFL) << 48) +
				   ((byteArray[i+0] & 0xFFL) << 56);
		return returnValue;
	}

	private long runTestCombineBE10(int i) {
		long returnValue = ((byteArray[i+7] & 0xFFL)                     ) |
				   ((byteArray[i+6] & 0xFFL) * 0x100L            ) |
				   ((byteArray[i+5] & 0xFFL) * 0x10000L          ) |
				   ((byteArray[i+4] & 0xFFL) * 0x1000000L        ) |
				   ((byteArray[i+3] & 0xFFL) * 0x100000000L      ) |
				   ((byteArray[i+2] & 0xFFL) * 0x10000000000L    ) |
				   ((byteArray[i+1] & 0xFFL) * 0x1000000000000L  ) |
				   ((byteArray[i+0] & 0xFFL) * 0x100000000000000L);
		return returnValue;
	}

	private int runTestCombineBE11(int i) {
		int returnValue = ((byteArray[3] & 0xFF)      ) +
				  ((byteArray[2] & 0xFF) <<  8) +
				  ((byteArray[1] & 0xFF) << 16) +
				  ((byteArray[0] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineBE12(int i) {
		int returnValue = ((byteArray[i-1] & 0xFF)      ) +
				  ((byteArray[i-2] & 0xFF) <<  8) +
				  ((byteArray[i-3] & 0xFF) << 16) +
				  ((byteArray[i-4] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestCombineBE13(int i) {
		int returnValue = ((byteArray[i+3] & 0xFF)      ) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) << 16) +
				  ((byteArray[i+0]       ) << 24);

		dummyIntArray[0] = returnValue;
		return returnValue;
	}

	private long runTestCombineBE14(int i) {
		long returnValue = ((byteArray[i+3] & 0xFFL)      ) +
				   ((byteArray[i+2] & 0xFFL) <<  8) +
				   ((byteArray[i+1] & 0xFFL) << 16) +
				   ((byteArray[i+0] & 0xFFL) << 24);

		dummyLongArray[0] = returnValue;
		return returnValue;
	}

	private long runTestCombineBE15(int i) {
		long returnValue = ((byteArray[i+2] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((byteArray[i+0] & 0xFFL) << 16);

		return returnValue;
	}

	private long runTestCombineBE16(int i) {
		long returnValue = ((byteArray[i+1] & 0xFFL)      ) +
				   ((byteArray[i+0] & 0xFFL) <<  8);

		return returnValue;
	}

	private int runTestCombineBE17(int i) {
		int returnValue = ((byteArray[i+1] & 0xFF)      ) +
				  ((byteArray[i+0]       ) <<  8);
		return returnValue;
	}

	private int runTestCombineBE18(int i) {
		int returnValue = ((byteArray[i+2] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+0]       ) << 16);
		return returnValue;
	}

	private long runTestCombineBE19(int i) {
		long returnValue = ((byteArray[i+3] & 0xFFL)      ) +
				   ((byteArray[i+2] & 0xFFL) <<  8) +
				   ((byteArray[i+1] & 0xFFL) << 16) +
				   ((long)(byteArray[i+0]  ) << 24);

		return returnValue;
	}

	private long runTestCombineBE20(int i) {
		long returnValue = ((byteArray[i+2] & 0xFFL)      ) +
				   ((byteArray[i+1] & 0xFFL) <<  8) +
				   ((long)(byteArray[i+0]  ) << 16);

		return returnValue;
	}

	private long runTestCombineBE21(int i) {
		long returnValue = ((byteArray[i+1] & 0xFFL)      ) +
				   ((long)(byteArray[i+0]  ) <<  8);

		return returnValue;
	}

	private int runTestCombineBE22(int i) {
		int returnValue = (((byteArray[i+3] & 0xFF)      ) +
				  ((byteArray[i+2] & 0xFF) <<  8)) +
				  (((byteArray[i+1] & 0xFF) << 16) +
				  ((byteArray[i+0] & 0xFF) << 24));
		return returnValue;
	}

	private long runTestCombineBE23(int i) {
		long returnValue = ((byteArray[i+4] & 0xFFL)      ) +
				   ((byteArray[i+3] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+1] & 0xFFL) << 24) +
				   ((byteArray[i+0] & 0xFFL) << 32);
		return returnValue;
	}

	private long runTestCombineBE24(int i) {
		long returnValue = ((byteArray[i+4] & 0xFFL)      ) +
				   ((byteArray[i+3] & 0xFFL) <<  8) +
				   ((byteArray[i+2] & 0xFFL) << 16) +
				   ((byteArray[i+1] & 0xFFL) << 24) +
				   ((long)(byteArray[i+0]  ) << 32);
		return returnValue;
	}

	private long runTestCombineBE25(int i) {
		long returnValue = ((byteArray[i+5] & 0xFFL)      ) +
				   ((byteArray[i+4] & 0xFFL) <<  8) +
				   ((byteArray[i+3] & 0xFFL) << 16) +
				   ((byteArray[i+2] & 0xFFL) << 24) +
				   ((byteArray[i+1] & 0xFFL) << 32) +
				   ((byteArray[i+0] & 0xFFL) << 40);
		return returnValue;
	}

	private long runTestCombineBE26(int i) {
		long returnValue = ((byteArray[i+5] & 0xFFL)      ) +
				   ((byteArray[i+4] & 0xFFL) <<  8) +
				   ((byteArray[i+3] & 0xFFL) << 16) +
				   ((byteArray[i+2] & 0xFFL) << 24) +
				   ((byteArray[i+1] & 0xFFL) << 32) +
				   ((long)(byteArray[i+0]  ) << 40);
		return returnValue;
	}

	private long runTestCombineBE27(int i) {
		long returnValue = ((byteArray[i+6] & 0xFFL)      ) +
				   ((byteArray[i+5] & 0xFFL) <<  8) +
				   ((byteArray[i+4] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((byteArray[i+2] & 0xFFL) << 32) +
				   ((byteArray[i+1] & 0xFFL) << 40) +
				   ((byteArray[i+0] & 0xFFL) << 48);
		return returnValue;
	}

	private long runTestCombineBE28(int i) {
		long returnValue = ((byteArray[i+6] & 0xFFL)      ) +
				   ((byteArray[i+5] & 0xFFL) <<  8) +
				   ((byteArray[i+4] & 0xFFL) << 16) +
				   ((byteArray[i+3] & 0xFFL) << 24) +
				   ((byteArray[i+2] & 0xFFL) << 32) +
				   ((byteArray[i+1] & 0xFFL) << 40) +
				   ((long)(byteArray[i+0]  ) << 48);
		return returnValue;
	}

	private long runTestCombineBE29(int i) {
		long returnValue = ((byteArray[i+10] & 0xFFL)      ) +
				   ((byteArray[i+9] & 0xFFL) <<  8) +
				   ((byteArray[i+8] & 0xFFL) << 16) +
				   ((byteArray[i+7] & 0xFFL) << 24) +
				   ((byteArray[i+6] & 0xFFL) << 32) +
				   ((byteArray[i+5] & 0xFFL) << 40) +
				   ((byteArray[i+4] & 0xFFL) << 48) +
				   ((byteArray[i+3] & 0xFFL) << 56);
		return returnValue;
	}

	private long runTestCombineBE30(int i) {
		long returnValue = ((byteArray[10] & 0xFFL)      ) +
				   ((byteArray[9] & 0xFFL) <<  8) +
				   ((byteArray[8] & 0xFFL) << 16) +
				   ((byteArray[7] & 0xFFL) << 24) +
				   ((byteArray[6] & 0xFFL) << 32) +
				   ((byteArray[5] & 0xFFL) << 40) +
				   ((byteArray[4] & 0xFFL) << 48) +
				   ((byteArray[3] & 0xFFL) << 56);
		return returnValue;
	}

	private long runTestCombineBE31(int i) {
		long returnValue = ((byteArray[15] & 0xFFL)      ) +
				   ((byteArray[14] & 0xFFL) <<  8) +
				   ((byteArray[13] & 0xFFL) << 16) +
				   ((byteArray[12] & 0xFFL) << 24) +
				   ((byteArray[11] & 0xFFL) << 32) +
				   ((byteArray[10] & 0xFFL) << 40) +
				   ((byteArray[9] & 0xFFL) << 48) +
				   ((byteArray[8] & 0xFFL) << 56);
		return returnValue;
	}

	private int runTestSplitLE01(int i) {
		int returnValue = ((byteArray[i+0]       )      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) << 16) +
				  ((byteArray[i+3] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitLE02(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) << 16) +
				  ((byteArray[i+4] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitLE03(int i) {
		int returnValue = ((byteArray[i+3] & 0xFF)      ) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) << 16) +
				  ((byteArray[i+0] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitLE04(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+3] & 0xFF) << 25);
		return returnValue;
	}

	private int runTestSplitLE05(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitLE06(int i) {
		int a = ((byteArray[i+0] & 0xFF)      );
		int b = ((byteArray[i+1] & 0xFF) <<  8);
		int c = ((byteArray[i+2] & 0xFF) << 16);
		int d = ((byteArray[i+3] & 0xFF) << 24);

		int returnValue = a + b + c + d;

		dummyLongArray[0] = (long)c;
		return returnValue;
	}

	private int runTestSplitLE07(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)         ) +
				  ((byteArray[i+1] & 0xFF) *  0x101) +
				  ((byteArray[i+2] & 0xFF) << 16   ) +
				  ((byteArray[i+3] & 0xFF) << 24   );
		return returnValue;
	}

	private int runTestSplitLE08(int i) {
		int returnValue = ((byteArray[i+0] & 0xF0)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) << 16) +
				  ((byteArray[i+3] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitLE09(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) << 16);
		return returnValue;
	}

	private int runTestSplitBE01(int i) {
		int returnValue = ((byteArray[i+3]       )      ) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) << 16) +
				  ((byteArray[i+0] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitBE02(int i) {
		int returnValue = ((byteArray[i+4] & 0xFF)      ) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) << 16) +
				  ((byteArray[i+0] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitBE03(int i) {
		int returnValue = ((byteArray[i+0] & 0xFF)      ) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+2] & 0xFF) << 16) +
				  ((byteArray[i+3] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitBE04(int i) {
		int returnValue = ((byteArray[i+3] & 0xFF)      ) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+0] & 0xFF) << 25);
		return returnValue;
	}

	private int runTestSplitBE05(int i) {
		int returnValue = ((byteArray[i+1] & 0xFF)      ) +
				  ((byteArray[i+0] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitBE06(int i) {
		int a = ((byteArray[i+3] & 0xFF)      );
		int b = ((byteArray[i+2] & 0xFF) <<  8);
		int c = ((byteArray[i+1] & 0xFF) << 16);
		int d = ((byteArray[i+0] & 0xFF) << 24);

		int returnValue = a + b + c + d;

		dummyLongArray[0] = (long)c;
		return returnValue;
	}

	private int runTestSplitBE07(int i) {
		int returnValue = ((byteArray[i+3] & 0xFF)         ) +
				  ((byteArray[i+2] & 0xFF) *  0x101) +
				  ((byteArray[i+1] & 0xFF) << 16   ) +
				  ((byteArray[i+0] & 0xFF) << 24   );
		return returnValue;
	}

	private int runTestSplitBE08(int i) {
		int returnValue = ((byteArray[i+3] & 0xF0)      ) +
				  ((byteArray[i+2] & 0xFF) <<  8) +
				  ((byteArray[i+1] & 0xFF) << 16) +
				  ((byteArray[i+0] & 0xFF) << 24);
		return returnValue;
	}

	private int runTestSplitBE09(int i) {
		int returnValue = ((byteArray[i+1] & 0xFF) <<  8) +
				  ((byteArray[i+0] & 0xFF) << 16);
		return returnValue;
	}
}
