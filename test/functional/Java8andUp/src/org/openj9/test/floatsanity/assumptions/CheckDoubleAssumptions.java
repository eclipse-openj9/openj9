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
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDoubleAssumptions {

	public static Logger logger = Logger.getLogger(CheckDoubleAssumptions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check double assumptions");
	}

	private ArrayList<Object[]> generator() {
		ArrayList<Object[]> tests = new ArrayList<>();

		tests.add(new Object[] {	"D.PZERO",		D.PZERO,	0x0000000000000000L	});
		tests.add(new Object[] {	"D.NZERO",		D.NZERO,	0x8000000000000000L	});
		tests.add(new Object[] {	"D.PINF",		D.PINF,		0x7FF0000000000000L	});
		tests.add(new Object[] {	"D.NINF",		D.NINF,		0xFFF0000000000000L	});
		tests.add(new Object[] {	"D.PMAX",		D.PMAX,		0x7FEFFFFFFFFFFFFFL	});
		tests.add(new Object[] {	"D.NMAX",		D.NMAX,		0xFFEFFFFFFFFFFFFFL	});
		tests.add(new Object[] {	"D.PMIN",		D.PMIN,		0x0000000000000001L	});
		tests.add(new Object[] {	"D.NMIN",		D.NMIN,		0x8000000000000001L	});
		tests.add(new Object[] {	"D.NAN",		D.NAN,		0x7FF8000000000000L	});

		tests.add(new Object[] {	"D.pOne",		D.pOne,		0x3FF0000000000000L	});
		tests.add(new Object[] {	"D.nOne",		D.nOne,		0xBFF0000000000000L	});
		tests.add(new Object[] {	"D.pTwo",		D.pTwo,		0x4000000000000000L	});
		tests.add(new Object[] {	"D.nTwo",		D.nTwo,		0xC000000000000000L	});
		tests.add(new Object[] {	"D.p1_1",		D.p1_1,		0x3FF199999999999AL	});
		tests.add(new Object[] {	"D.n1_1",		D.n1_1,		0xBFF199999999999AL	});
		tests.add(new Object[] {	"D.p2_2",		D.p2_2,		0x400199999999999AL	});
		tests.add(new Object[] {	"D.n2_2",		D.n2_2,		0xC00199999999999AL	});
		tests.add(new Object[] {	"D.p100",		D.p100,		0x4059000000000000L	});
		tests.add(new Object[] {	"D.n100",		D.n100,		0xC059000000000000L	});
		tests.add(new Object[] {	"D.p200",		D.p200,		0x4069000000000000L	});
		tests.add(new Object[] {	"D.n200",		D.n200,		0xC069000000000000L	});
		tests.add(new Object[] {	"D.p1000",		D.p1000,	0x408F400000000000L	});
		tests.add(new Object[] {	"D.n1000",		D.n1000,	0xC08F400000000000L	});
		tests.add(new Object[] {	"D.p2000",		D.p2000,	0x409F400000000000L	});
		tests.add(new Object[] {	"D.n2000",		D.n2000,	0xC09F400000000000L	});
		tests.add(new Object[] {	"D.p0_5",		D.p0_5,		0x3FE0000000000000L	});
		tests.add(new Object[] {	"D.n0_5",		D.n0_5,		0xBFE0000000000000L	});
		tests.add(new Object[] {	"D.p0_01",		D.p0_01,	0x3F847AE147AE147BL	});
		tests.add(new Object[] {	"D.n0_01",		D.n0_01,	0xBF847AE147AE147BL	});
		tests.add(new Object[] {	"D.p0_005",		D.p0_005,	0x3F747AE147AE147BL	});
		tests.add(new Object[] {	"D.n0_005",		D.n0_005,	0xBF747AE147AE147BL	});
		tests.add(new Object[] {	"D.p0_0001",	D.p0_0001,	0x3F1A36E2EB1C432DL	});
		tests.add(new Object[] {	"D.n0_0001",	D.n0_0001,	0xBF1A36E2EB1C432DL	});
		tests.add(new Object[] {	"D.p0_0002",	D.p0_0002,	0x3F2A36E2EB1C432DL	});
		tests.add(new Object[] {	"D.n0_0002",	D.n0_0002,	0xBF2A36E2EB1C432DL	});
		tests.add(new Object[] {	"D.pC1",		D.pC1,		0x0004000000000000L	});
		tests.add(new Object[] {	"D.nC1",		D.nC1,		0x8004000000000000L	});
		tests.add(new Object[] {	"D.p1075",		D.p1075,	0x4090CC0000000000L	});
		tests.add(new Object[] {	"D.n1075",		D.n1075,	0xC090CC0000000000L	});

		tests.add(new Object[] {	"D.pBMAX",		D.pBMAX,	0x405FC00000000000L	});
		tests.add(new Object[] {	"D.nBMAX",		D.nBMAX,	0xC060000000000000L	});
		tests.add(new Object[] {	"D.pSMAX",		D.pSMAX,	0x40DFFFC000000000L	});
		tests.add(new Object[] {	"D.nSMAX",		D.nSMAX,	0xC0E0000000000000L	});
		tests.add(new Object[] {	"D.pIMAX",		D.pIMAX,	0x41DFFFFFFFC00000L	});
		tests.add(new Object[] {	"D.nIMAX",		D.nIMAX,	0xC1E0000000000000L	});
		tests.add(new Object[] {	"D.pLMAX",		D.pLMAX,	0x43E0000000000000L	});
		tests.add(new Object[] {	"D.nLMAX",		D.nLMAX,	0xC3E0000000000000L	});

		return tests;
	}

	public void double_constant_assumptions() {
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
}
