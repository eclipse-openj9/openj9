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

import org.openj9.test.floatsanity.D;
import org.openj9.test.floatsanity.F;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckConversionsToDouble {
	
	public static final byte pBMAX = (byte)0x7F;
	public static final byte nBMAX = (byte)0x80;
	public static final short pSMAX = (short)0x7FFF;
	public static final short nSMAX = (short)0x8000;
	public static final int pIMAX = 0x7FFFFFFF;
	public static final int nIMAX = 0x80000000;
	public static final long pLMAX = 0x7FFFFFFFFFFFFFFFL;
	public static final long nLMAX = 0x8000000000000000L;

	class ConversionPair<T> {
		public T from;
		public double to;

		public ConversionPair(T from, double to) {
			this.from = from;
			this.to = to;
		}
	}

	public static Logger logger = Logger.getLogger(CheckConversionsToDouble.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check conversions to double");
	}

	public void byte_to_double() {
		ArrayList<ConversionPair<Byte>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Byte>((byte)0,		D.PZERO));
		testCases.add(new ConversionPair<Byte>((byte)1,		D.pOne));
		testCases.add(new ConversionPair<Byte>((byte)-1,	D.nOne));
		testCases.add(new ConversionPair<Byte>((byte)pBMAX,	D.pBMAX));
		testCases.add(new ConversionPair<Byte>((byte)nBMAX,	D.nBMAX));

		for (ConversionPair<Byte> pair : testCases) {
			String operation = "testing conversion: convert byte " + pair.from + " to double";
			logger.debug(operation);
			Assert.assertEquals((double)pair.from, pair.to, 0, operation);
		}
	}

	public void char_to_double() {
		ArrayList<ConversionPair<Character>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Character>((char)0,		D.PZERO));
		testCases.add(new ConversionPair<Character>((char)1,		D.pOne));
		testCases.add(new ConversionPair<Character>((char)pBMAX,	D.pBMAX));
		testCases.add(new ConversionPair<Character>((char)pSMAX,	D.pSMAX));

		for (ConversionPair<Character> pair : testCases) {
			String operation = "testing conversion: convert char " + pair.from + " to double";
			logger.debug(operation);
			Assert.assertEquals((double)pair.from, pair.to, 0, operation);
		}
	}

	public void short_to_double() {
		ArrayList<ConversionPair<Short>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Short>((short)0,		D.PZERO));
		testCases.add(new ConversionPair<Short>((short)1,		D.pOne));
		testCases.add(new ConversionPair<Short>((short)-1,		D.nOne));
		testCases.add(new ConversionPair<Short>((short)pBMAX,	D.pBMAX));
		testCases.add(new ConversionPair<Short>((short)nBMAX,	D.nBMAX));
		testCases.add(new ConversionPair<Short>((short)pSMAX,	D.pSMAX));
		testCases.add(new ConversionPair<Short>((short)nSMAX,	D.nSMAX));

		for (ConversionPair<Short> pair : testCases) {
			String operation = "testing conversion: convert short " + pair.from + " to double";
			logger.debug(operation);
			Assert.assertEquals((double)pair.from, pair.to, 0, operation);
		}
	}

	public void int_to_double() {
		ArrayList<ConversionPair<Integer>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Integer>(0,			D.PZERO));
		testCases.add(new ConversionPair<Integer>(1,			D.pOne));
		testCases.add(new ConversionPair<Integer>(-1,			D.nOne));
		testCases.add(new ConversionPair<Integer>((int)pBMAX,	D.pBMAX));
		testCases.add(new ConversionPair<Integer>((int)nBMAX,	D.nBMAX));
		testCases.add(new ConversionPair<Integer>((int)pSMAX,	D.pSMAX));
		testCases.add(new ConversionPair<Integer>((int)nSMAX,	D.nSMAX));
		testCases.add(new ConversionPair<Integer>((int)pIMAX,	D.pIMAX));
		testCases.add(new ConversionPair<Integer>((int)nIMAX,	D.nIMAX));

		for (ConversionPair<Integer> pair : testCases) {
			String operation = "testing conversion: convert int " + pair.from + " to double";
			logger.debug(operation);
			Assert.assertEquals((double)pair.from, pair.to, 0, operation);
		}
	}

	public void long_to_double() {
		ArrayList<ConversionPair<Long>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Long>(0L,				D.PZERO));
		testCases.add(new ConversionPair<Long>(1L,				D.pOne));
		testCases.add(new ConversionPair<Long>(-1L,				D.nOne));
		testCases.add(new ConversionPair<Long>((long)pBMAX,		D.pBMAX));
		testCases.add(new ConversionPair<Long>((long)nBMAX,		D.nBMAX));
		testCases.add(new ConversionPair<Long>((long)pSMAX,		D.pSMAX));
		testCases.add(new ConversionPair<Long>((long)nSMAX,		D.nSMAX));
		testCases.add(new ConversionPair<Long>((long)pIMAX,		D.pIMAX));
		testCases.add(new ConversionPair<Long>((long)nIMAX,		D.nIMAX));
		testCases.add(new ConversionPair<Long>((long)pLMAX,		D.pLMAX));
		testCases.add(new ConversionPair<Long>((long)nLMAX,		D.nLMAX));

		for (ConversionPair<Long> pair : testCases) {
			String operation = "testing conversion: convert long " + pair.from + " to double";
			logger.debug(operation);
			Assert.assertEquals((double)pair.from, pair.to, 0, operation);
		}
	}

	public void float_to_double() {
		ArrayList<ConversionPair<Float>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Float>(F.PZERO,	D.PZERO));
		testCases.add(new ConversionPair<Float>(F.NZERO,	D.NZERO));
		testCases.add(new ConversionPair<Float>(F.PINF,		D.PINF));
		testCases.add(new ConversionPair<Float>(F.NINF,		D.NINF));

		/* Careful: the double constant may encode the decimal value
		   to greater precision than its float counterpart.  These
		   constant pairs have been selected because the double has
		   no more precision than the float. 
		*/
		testCases.add(new ConversionPair<Float>(F.pOne,		D.pOne));
		testCases.add(new ConversionPair<Float>(F.nOne,		D.nOne));
		testCases.add(new ConversionPair<Float>(F.pTwo,		D.pTwo));
		testCases.add(new ConversionPair<Float>(F.nTwo,		D.nTwo));
		testCases.add(new ConversionPair<Float>(F.p200,		D.p200));
		testCases.add(new ConversionPair<Float>(F.n200,		D.n200));
		testCases.add(new ConversionPair<Float>(F.nBMAX,	D.nBMAX));
		testCases.add(new ConversionPair<Float>(F.nSMAX,	D.nSMAX));
		testCases.add(new ConversionPair<Float>(F.nIMAX,	D.nIMAX));
		testCases.add(new ConversionPair<Float>(F.nLMAX,	D.nLMAX));

		String operation;
		for (ConversionPair<Float> pair : testCases) {
			operation = "testing conversion: convert float " + pair.from + " to double";
			logger.debug(operation);
			Assert.assertEquals((double)pair.from, pair.to, 0, operation);
		}
		operation = "testing conversion: convert float " + F.NAN + " to double";
		logger.debug(operation);
		Assert.assertTrue(Double.isNaN((double)F.NAN));
	}
}
