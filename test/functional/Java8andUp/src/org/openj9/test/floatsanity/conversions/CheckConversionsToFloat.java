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
import org.openj9.test.floatsanity.conversions.CheckConversionsToDouble.ConversionPair;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckConversionsToFloat {

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
		public float to;

		public ConversionPair(T from, Float to) {
			this.from = from;
			this.to = to;
		}
	}

	public static Logger logger = Logger.getLogger(CheckConversionsToFloat.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check conversions to float");
	}

	public void byte_to_float() {
		ArrayList<ConversionPair<Byte>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Byte>((byte)0,			F.PZERO));
		testCases.add(new ConversionPair<Byte>((byte)1,			F.pOne));
		testCases.add(new ConversionPair<Byte>((byte)-1,		F.nOne));
		testCases.add(new ConversionPair<Byte>((byte)pBMAX,		F.pBMAX));
		testCases.add(new ConversionPair<Byte>((byte)nBMAX,		F.nBMAX));

		for (ConversionPair<Byte> pair : testCases) {
			String operation = "testing conversion: convert byte " + pair.from + " to float";
			logger.debug(operation);
			Assert.assertEquals((float)pair.from, pair.to, 0, operation);
		}
	}

	public void char_to_float() {
		ArrayList<ConversionPair<Character>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Character>((char)0,		F.PZERO));
		testCases.add(new ConversionPair<Character>((char)1,		F.pOne));
		testCases.add(new ConversionPair<Character>((char)pBMAX,	F.pBMAX));
		testCases.add(new ConversionPair<Character>((char)pSMAX,	F.pSMAX));

		for (ConversionPair<Character> pair : testCases) {
			String operation = "testing conversion: convert char " + pair.from + " to float";
			logger.debug(operation);
			Assert.assertEquals((float)pair.from, pair.to, 0, operation);
		}
	}

	public void short_to_float() {
		ArrayList<ConversionPair<Short>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Short>((short)0,		F.PZERO));
		testCases.add(new ConversionPair<Short>((short)1,		F.pOne));
		testCases.add(new ConversionPair<Short>((short)-1,		F.nOne));
		testCases.add(new ConversionPair<Short>((short)pBMAX,	F.pBMAX));
		testCases.add(new ConversionPair<Short>((short)nBMAX,	F.nBMAX));
		testCases.add(new ConversionPair<Short>((short)pSMAX,	F.pSMAX));
		testCases.add(new ConversionPair<Short>((short)nSMAX,	F.nSMAX));

		for (ConversionPair<Short> pair : testCases) {
			String operation = "testing conversion: convert short " + pair.from + " to float";
			logger.debug(operation);
			Assert.assertEquals((float)pair.from, pair.to, 0, operation);
		}
	}

	public void int_to_float() {
		ArrayList<ConversionPair<Integer>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Integer>(0,			F.PZERO));
		testCases.add(new ConversionPair<Integer>(1,			F.pOne));
		testCases.add(new ConversionPair<Integer>(-1,			F.nOne));
		testCases.add(new ConversionPair<Integer>((int)pBMAX,	F.pBMAX));
		testCases.add(new ConversionPair<Integer>((int)nBMAX,	F.nBMAX));
		testCases.add(new ConversionPair<Integer>((int)pSMAX,	F.pSMAX));
		testCases.add(new ConversionPair<Integer>((int)nSMAX,	F.nSMAX));
		testCases.add(new ConversionPair<Integer>((int)pIMAX,	F.pIMAX));
		testCases.add(new ConversionPair<Integer>((int)nIMAX,	F.nIMAX));

		for (ConversionPair<Integer> pair : testCases) {
			String operation = "testing conversion: convert int " + pair.from + " to float";
			logger.debug(operation);
			Assert.assertEquals((float)pair.from, pair.to, 0, operation);
		}
	}

	public void long_to_float() {
		ArrayList<ConversionPair<Long>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Long>(0L,				F.PZERO));
		testCases.add(new ConversionPair<Long>(1L,				F.pOne));
		testCases.add(new ConversionPair<Long>(-1L,				F.nOne));
		testCases.add(new ConversionPair<Long>((long)pBMAX,		F.pBMAX));
		testCases.add(new ConversionPair<Long>((long)nBMAX,		F.nBMAX));
		testCases.add(new ConversionPair<Long>((long)pSMAX,		F.pSMAX));
		testCases.add(new ConversionPair<Long>((long)nSMAX,		F.nSMAX));
		testCases.add(new ConversionPair<Long>((long)pIMAX,		F.pIMAX));
		testCases.add(new ConversionPair<Long>((long)nIMAX,		F.nIMAX));
		testCases.add(new ConversionPair<Long>((long)pLMAX,		F.pLMAX));
		testCases.add(new ConversionPair<Long>((long)nLMAX,		F.nLMAX));

		for (ConversionPair<Long> pair : testCases) {
			String operation = "testing conversion: convert long " + pair.from + " to float";
			logger.debug(operation);
			Assert.assertEquals((float)pair.from, pair.to, 0, operation);
		}
	}

	public void double_to_float() {
		ArrayList<ConversionPair<Double>> testCases = new ArrayList<>();

		testCases.add(new ConversionPair<Double>(D.PZERO,	F.PZERO));
		testCases.add(new ConversionPair<Double>(D.NZERO,	F.NZERO));
		testCases.add(new ConversionPair<Double>(D.PINF,	F.PINF));
		testCases.add(new ConversionPair<Double>(D.NINF,	F.NINF));
		testCases.add(new ConversionPair<Double>(D.PMAX,	F.PINF));
		testCases.add(new ConversionPair<Double>(D.NMAX,	F.NINF));
		testCases.add(new ConversionPair<Double>(D.PMIN,	F.PZERO));
		testCases.add(new ConversionPair<Double>(D.NMIN,	F.NZERO));

		/* These constant pairs were chosen because the floats
		   round properly when converted back to float.  Some types
		   of non-finites (denormal?) are not represented here. */
		testCases.add(new ConversionPair<Double>(D.p100,	F.p100));
		testCases.add(new ConversionPair<Double>(D.n100,	F.n100));
		testCases.add(new ConversionPair<Double>(D.p200,	F.p200));
		testCases.add(new ConversionPair<Double>(D.n200,	F.n200));
		testCases.add(new ConversionPair<Double>(D.p1000,	F.p1000));
		testCases.add(new ConversionPair<Double>(D.n1000,	F.n1000));
		testCases.add(new ConversionPair<Double>(D.nBMAX,	F.nBMAX));
		testCases.add(new ConversionPair<Double>(D.nSMAX,	F.nSMAX));
		testCases.add(new ConversionPair<Double>(D.nIMAX,	F.nIMAX));
		testCases.add(new ConversionPair<Double>(D.nLMAX,	F.nLMAX));

		String operation;
		for (ConversionPair<Double> pair : testCases) {
			operation = "testing conversion: convert double " + pair.from + " to float";
			logger.debug(operation);
			Assert.assertEquals((float)(double)pair.from, pair.to, 0, operation);
		}
		operation = "testing conversion: convert double " + D.NAN + " to float";
		logger.debug(operation);
		Assert.assertTrue(Float.isNaN((float)F.NAN));
	}
}
