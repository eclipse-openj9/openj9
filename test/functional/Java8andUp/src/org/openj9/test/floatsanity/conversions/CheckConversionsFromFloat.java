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
package org.openj9.test.floatsanity.conversions;

import java.util.ArrayList;

import org.openj9.test.floatsanity.F;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckConversionsFromFloat {

	public static final byte pBMAX = (byte)0x7F;
	public static final byte nBMAX = (byte)0x80;
	public static final short pSMAX = (short)0x7FFF;
	public static final short nSMAX = (short)0x8000;
	public static final int pIMAX = 0x7FFFFFFF;
	public static final int nIMAX = 0x80000000;
	public static final long pLMAX = 0x7FFFFFFFFFFFFFFFL;
	public static final long nLMAX = 0x8000000000000000L;

	class ConversionPair<T> {
		public float from;
		public T to;

		public ConversionPair(float from, T to) {
			this.from = from;
			this.to = to;
		}
	}

	public static Logger logger = Logger.getLogger(CheckConversionsFromFloat.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check conversions from float");
	}

	/* Sun describes saturation semantics for
		"narrowing conversions from float to integral type"
		(which they	define as "float to byte, short, char int or long")
		which are impossible to implement without dedicated bytecodes
		f2b, f2s and f2c.  The current combinations used to do those
		conversions (f2i, i2b etc) result in 0 for float values that were
		*too negative*, and -1 (or 65535 if converting to char) for values
		that were *too positive*. 
	*/

	public void float_to_byte() {
		ArrayList<ConversionPair<Byte>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Byte>(F.PZERO,	 (byte)0));
		testCases.add(new ConversionPair<Byte>(F.NZERO,	 (byte)0));
		testCases.add(new ConversionPair<Byte>(F.pOne,	 (byte)1));
		testCases.add(new ConversionPair<Byte>(F.nOne,	 (byte)-1));
		testCases.add(new ConversionPair<Byte>(F.pBMAX,	 (byte)pBMAX));
		testCases.add(new ConversionPair<Byte>(F.nBMAX,	 (byte)nBMAX));
		testCases.add(new ConversionPair<Byte>(F.PINF,	 (byte)0xFF));
		testCases.add(new ConversionPair<Byte>(F.NINF,	 (byte)0x00));
		testCases.add(new ConversionPair<Byte>(F.PMAX,	 (byte)0xFF));
		testCases.add(new ConversionPair<Byte>(F.NMAX,	 (byte)0x00));
		testCases.add(new ConversionPair<Byte>(F.PMIN,	 (byte)0));
		testCases.add(new ConversionPair<Byte>(F.NMIN,	 (byte)0));
		
		for (ConversionPair<Byte> pair : testCases) {
			String operation = "testing conversion: convert float " + pair.from + " to byte";
			logger.debug(operation);
			Assert.assertEquals((byte)pair.from, (byte)pair.to, operation);
		}
	}

	public void float_to_char() {
		ArrayList<ConversionPair<Character>> testCases = new ArrayList<>();

 		testCases.add(new ConversionPair<Character>(F.PZERO,	 (char)0));
		testCases.add(new ConversionPair<Character>(F.NZERO,	 (char)0));
		testCases.add(new ConversionPair<Character>(F.pOne,		 (char)1));
		testCases.add(new ConversionPair<Character>(F.pBMAX,	 (char)pBMAX));
		testCases.add(new ConversionPair<Character>(F.pSMAX,	 (char)pSMAX));
		testCases.add(new ConversionPair<Character>(F.PINF,		 (char)0xFFFF));
		testCases.add(new ConversionPair<Character>(F.NINF,		 (char)0x0000));
		testCases.add(new ConversionPair<Character>(F.PMAX,		 (char)0xFFFF));
		testCases.add(new ConversionPair<Character>(F.NMAX,		 (char)0x0000));
		testCases.add(new ConversionPair<Character>(F.PMIN,		 (char)0));
		testCases.add(new ConversionPair<Character>(F.NMIN,		 (char)0));

		for (ConversionPair<Character> pair : testCases) {
			String operation = "testing conversion: convert float " + pair.from + " to char";
			logger.debug(operation);
			Assert.assertEquals((char)pair.from, (char)pair.to, operation);
		}
	}

	public void float_to_short() {
		ArrayList<ConversionPair<Short>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Short>(F.PZERO,	 (short)0));
		testCases.add(new ConversionPair<Short>(F.NZERO,	 (short)0));
		testCases.add(new ConversionPair<Short>(F.pOne,		 (short)1));
		testCases.add(new ConversionPair<Short>(F.nOne,		 (short)-1));
		testCases.add(new ConversionPair<Short>(F.pBMAX,	 (short)pBMAX));
		testCases.add(new ConversionPair<Short>(F.nBMAX,	 (short)nBMAX));
		testCases.add(new ConversionPair<Short>(F.pSMAX,	 (short)pSMAX));
		testCases.add(new ConversionPair<Short>(F.nSMAX,	 (short)nSMAX));
		testCases.add(new ConversionPair<Short>(F.PINF,		 (short)0xFFFF));
		testCases.add(new ConversionPair<Short>(F.NINF,		 (short)0x0000));
		testCases.add(new ConversionPair<Short>(F.PMAX,		 (short)0xFFFF));
		testCases.add(new ConversionPair<Short>(F.NMAX,		 (short)0x0000));
		testCases.add(new ConversionPair<Short>(F.PMIN,		 (short)0));
		testCases.add(new ConversionPair<Short>(F.NMIN,		 (short)0));

		for (ConversionPair<Short> pair : testCases) {
			String operation = "testing conversion: convert float " + pair.from + " to short";
			logger.debug(operation);
			Assert.assertEquals((short)pair.from, (short)pair.to, operation);
		}
	}

	public void float_to_int() {
		ArrayList<ConversionPair<Integer>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Integer>(F.PZERO,	0));
		testCases.add(new ConversionPair<Integer>(F.NZERO,	0));
		testCases.add(new ConversionPair<Integer>(F.pOne,	1));
		testCases.add(new ConversionPair<Integer>(F.nOne,	-1));
		testCases.add(new ConversionPair<Integer>(F.pBMAX,	(int)pBMAX));
		testCases.add(new ConversionPair<Integer>(F.nBMAX,	(int)nBMAX));
		testCases.add(new ConversionPair<Integer>(F.pSMAX,	(int)pSMAX));
		testCases.add(new ConversionPair<Integer>(F.nSMAX,	(int)nSMAX));
		testCases.add(new ConversionPair<Integer>(F.nIMAX,	(int)nIMAX));
		testCases.add(new ConversionPair<Integer>(F.PINF,	(int)pIMAX));
		testCases.add(new ConversionPair<Integer>(F.NINF,	(int)nIMAX));
		testCases.add(new ConversionPair<Integer>(F.PMAX,	(int)pIMAX));
		testCases.add(new ConversionPair<Integer>(F.NMAX,	(int)nIMAX));
		testCases.add(new ConversionPair<Integer>(F.PMIN,	0));
		testCases.add(new ConversionPair<Integer>(F.NMIN,	0));

		for (ConversionPair<Integer> pair : testCases) {
			String operation = "testing conversion: convert float " + pair.from + " to byintte";
			logger.debug(operation);
			Assert.assertEquals((int)pair.from, (int)pair.to, operation);
		}
	}

	public void float_to_long() {
		ArrayList<ConversionPair<Long>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Long>(F.PZERO,	0L));
		testCases.add(new ConversionPair<Long>(F.NZERO,	0L));
		testCases.add(new ConversionPair<Long>(F.pOne,	1L));
		testCases.add(new ConversionPair<Long>(F.nOne,	-1L));
		testCases.add(new ConversionPair<Long>(F.pBMAX,	(long)pBMAX));
		testCases.add(new ConversionPair<Long>(F.nBMAX,	(long)nBMAX));
		testCases.add(new ConversionPair<Long>(F.pSMAX,	(long)pSMAX));
		testCases.add(new ConversionPair<Long>(F.nSMAX,	(long)nSMAX));
		testCases.add(new ConversionPair<Long>(F.nIMAX,	(long)nIMAX));
		testCases.add(new ConversionPair<Long>(F.nLMAX,	(long)nLMAX));
		testCases.add(new ConversionPair<Long>(F.PINF,	(long)pLMAX));
		testCases.add(new ConversionPair<Long>(F.NINF,	(long)nLMAX));
		testCases.add(new ConversionPair<Long>(F.PMAX,	(long)pLMAX));
		testCases.add(new ConversionPair<Long>(F.NMAX,	(long)nLMAX));
		testCases.add(new ConversionPair<Long>(F.PMIN,	0L));
		testCases.add(new ConversionPair<Long>(F.NMIN,	0L));

		for (ConversionPair<Long> pair : testCases) {
			String operation = "testing conversion: convert float " + pair.from + " to long";
			logger.debug(operation);
			Assert.assertEquals((long)pair.from, (long)pair.to, operation);
		}
	}

}
