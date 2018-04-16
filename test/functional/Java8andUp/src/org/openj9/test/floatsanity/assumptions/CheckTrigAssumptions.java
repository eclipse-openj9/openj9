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
package org.openj9.test.floatsanity.assumptions;

import java.util.ArrayList;

import org.openj9.test.floatsanity.D;
import org.openj9.test.floatsanity.TD;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckTrigAssumptions {

	public static Logger logger = Logger.getLogger(CheckTrigAssumptions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check trig double assumptions");
	}

	private ArrayList<Object[]> generator() {
		ArrayList<Object[]> tests = new ArrayList<>();

		tests.add(new Object[] {	"constant: TD.PPI",		TD.PPI,		0x400921FB54442D18L	});
		tests.add(new Object[] {	"constant: TD.NPI",		TD.NPI,		0xC00921FB54442D18L	});
		tests.add(new Object[] {	"constant: TD.P2PI",	TD.P2PI,	0x401921FB54442D18L	});
		tests.add(new Object[] {	"constant: TD.N2PI",	TD.N2PI,	0xC01921FB54442D18L	});
		tests.add(new Object[] {	"constant: TD.PHPI",	TD.PHPI,	0x3FF921FB54442D18L	});
		tests.add(new Object[] {	"constant: TD.NHPI",	TD.NHPI,	0xBFF921FB54442D18L	});
		tests.add(new Object[] {	"constant: TD.P3HPI",	TD.P3HPI,	0x4012D97C7F3321D2L	});
		tests.add(new Object[] {	"constant: TD.N3HPI",	TD.N3HPI,	0xC012D97C7F3321D2L	});

		for (int i = 0; i<degValue.length; i++)  {
			tests.add(new Object[] { "angle: TD.p" + i * 15 + "deg", TD.degArray[i], degValue[i] });
			tests.add(new Object[] { "angle: TD.p" + i * 15 + "rad", TD.radArray[i], radValue[i] });
		}

		for (int i = 0; i<ndegValue.length; i++)  {
			tests.add(new Object[] { "angle: TD.n" + i * 15 + "deg", TD.ndegArray[i], ndegValue[i] });
			tests.add(new Object[] { "angle: TD.n" + i * 15 + "rad", TD.nradArray[i], nradValue[i] });
		}

		tests.add(new Object[] {	"sin: TD.pS0",	TD.pS0,		0x0000000000000000L	});
		tests.add(new Object[] {	"sin: TD.pS15",	TD.pS15,	0x3FD0907DC1930690L	});
		tests.add(new Object[] {	"sin: TD.pS30",	TD.pS30,	0x3FE0000000000000L	});
		tests.add(new Object[] {	"sin: TD.pS45",	TD.pS45,	0x3FE6A09E667F3BCCL	});
		tests.add(new Object[] {	"sin: TD.pS60",	TD.pS60,	0x3FEBB67AE8584CAAL	});
		tests.add(new Object[] {	"sin: TD.pS75",	TD.pS75,	0x3FEEE8DD4748BF15L	});
		tests.add(new Object[] {	"sin: TD.pS90",	TD.pS90,	0x3FF0000000000000L	});
		tests.add(new Object[] {	"sin: TD.nS0",	TD.nS0,		0x8000000000000000L	});
		tests.add(new Object[] {	"sin: TD.nS15",	TD.nS15,	0xBFD0907DC1930690L	});
		tests.add(new Object[] {	"sin: TD.nS30",	TD.nS30,	0xBFE0000000000000L	});
		tests.add(new Object[] {	"sin: TD.nS45",	TD.nS45,	0xBFE6A09E667F3BCCL	});
		tests.add(new Object[] {	"sin: TD.nS60",	TD.nS60,	0xBFEBB67AE8584CAAL	});
		tests.add(new Object[] {	"sin: TD.nS75",	TD.nS75,	0xBFEEE8DD4748BF15L	});
		tests.add(new Object[] {	"sin: TD.nS90",	TD.nS90,	0xBFF0000000000000L	});

		tests.add(new Object[] {	"tan: TD.pT0",	TD.pT0,		0x0000000000000000L	});
		tests.add(new Object[] {	"tan: TD.pT15",	TD.pT15,	0x3FD126145E9ECD56L	});
		tests.add(new Object[] {	"tan: TD.pT30",	TD.pT30,	0x3FE279A74590331DL	});
		tests.add(new Object[] {	"tan: TD.pT45",	TD.pT45,	0x3FF0000000000000L	});
		tests.add(new Object[] {	"tan: TD.pT60",	TD.pT60,	0x3FFBB67AE8584CAAL	});
		tests.add(new Object[] {	"tan: TD.pT75",	TD.pT75,	0x400DDB3D742C2656L	});
		tests.add(new Object[] {	"tan: TD.pT90",	TD.pT90,	0x7FF0000000000000L	});

		return tests;
	}

	public void trig_double_assumptions() {
		ArrayList<Object[]> testCases = generator();
		for (Object[] args : testCases) {
			String name = (String)args[0];
			double assumed = (double)args[1];
			long expectedLong = (long)args[2];
			double expected = D.L2D(expectedLong);
			String operation = "checking assumption " + name + " is equal to 0x" + Long.toHexString(expectedLong);
			logger.debug(operation);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(assumed), operation);
			} else {
				Assert.assertEquals(assumed, expected, 0, operation);
			}
		}
	}

	static final long[] degValue = {
		0x0000000000000000L, 0x402E000000000000L, 0x403E000000000000L, 0x4046800000000000L,
		0x404E000000000000L, 0x4052C00000000000L, 0x4056800000000000L, 0x405A400000000000L,
		0x405E000000000000L, 0x4060E00000000000L, 0x4062C00000000000L, 0x4064A00000000000L,
		0x4066800000000000L, 0x4068600000000000L, 0x406A400000000000L, 0x406C200000000000L,
		0x406E000000000000L, 0x406FE00000000000L, 0x4070E00000000000L, 0x4071D00000000000L,
		0x4072C00000000000L, 0x4073B00000000000L, 0x4074A00000000000L, 0x4075900000000000L,
		0x4076800000000000L
	};
	static final long[] ndegValue = {
		0x8000000000000000L, 0xC02E000000000000L, 0xC03E000000000000L, 0xC046800000000000L,
		0xC04E000000000000L, 0xC052C00000000000L, 0xC056800000000000L, 0xC05A400000000000L,
		0xC05E000000000000L, 0xC060E00000000000L, 0xC062C00000000000L, 0xC064A00000000000L,
		0xC066800000000000L
	};
	static final long[] radValue = {
		0x0000000000000000L, 0x3FD0C152382D7365L, 0x3FE0C152382D7365L, 0x3FE921FB54442D18L,
		0x3FF0C152382D7365L, 0x3FF4F1A6C638D03FL, 0x3FF921FB54442D18L, 0x3FFD524FE24F89F2L,
		0x4000C152382D7365L, 0x4002D97C7F3321D2L, 0x4004F1A6C638D03FL, 0x400709D10D3E7EABL,
		0x400921FB54442D18L, 0x400B3A259B49DB84L, 0x400D524FE24F89F2L, 0x400F6A7A2955385EL,
		0x4010C152382D7365L, 0x4011CD675BB04A9CL, 0x4012D97C7F3321D2L, 0x4013E591A2B5F909L,
		0x4014F1A6C638D03FL, 0x4015FDBBE9BBA775L, 0x401709D10D3E7EABL, 0x401815E630C155E2L,
		0x401921FB54442D18L
	};
	static final long[] nradValue = {
		0x8000000000000000L, 0xBFD0C152382D7365L, 0xBFE0C152382D7365L, 0xBFE921FB54442D18L,
		0xBFF0C152382D7365L, 0xBFF4F1A6C638D03FL, 0xBFF921FB54442D18L, 0xBFFD524FE24F89F2L,
		0xC000C152382D7365L, 0xC002D97C7F3321D2L, 0xC004F1A6C638D03FL, 0xC00709D10D3E7EABL,
		0xC00921FB54442D18L
	};
}
