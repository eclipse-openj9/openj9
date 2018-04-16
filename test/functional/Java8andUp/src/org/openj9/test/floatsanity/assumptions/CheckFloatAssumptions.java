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

import org.openj9.test.floatsanity.F;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;


@Test(groups = { "level.sanity" })
public class CheckFloatAssumptions {

	public static Logger logger = Logger.getLogger(CheckFloatAssumptions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check float assumptions");
	}

	private ArrayList<Object[]> generator() {
		ArrayList<Object[]> tests = new ArrayList<>();

		tests.add(new Object[] {	"F.PZERO",		F.PZERO,	0x00000000	});
		tests.add(new Object[] {	"F.NZERO",		F.NZERO,	0x80000000	});
		tests.add(new Object[] {	"F.PINF",		F.PINF,		0x7F800000	});
		tests.add(new Object[] {	"F.NINF",		F.NINF,		0xFF800000	});
		tests.add(new Object[] {	"F.PMAX",		F.PMAX,		0x7F7FFFFF	});
		tests.add(new Object[] {	"F.NMAX",		F.NMAX,		0xFF7FFFFF	});
		tests.add(new Object[] {	"F.PMIN",		F.PMIN,		0x00000001	});
		tests.add(new Object[] {	"F.NMIN",		F.NMIN,		0x80000001	});
		tests.add(new Object[] {	"F.NAN",		F.NAN,		0x7FC00000	});

		tests.add(new Object[] {	"F.pOne",		F.pOne,		0x3F800000	});
		tests.add(new Object[] {	"F.nOne",		F.nOne,		0xBF800000	});
		tests.add(new Object[] {	"F.pTwo",		F.pTwo,		0x40000000	});
		tests.add(new Object[] {	"F.nTwo",		F.nTwo,		0xC0000000	});
		tests.add(new Object[] {	"F.p1_1",		F.p1_1,		0x3F8CCCCD	});
		tests.add(new Object[] {	"F.n1_1",		F.n1_1,		0xBF8CCCCD	});
		tests.add(new Object[] {	"F.p2_2",		F.p2_2,		0x400CCCCD	});
		tests.add(new Object[] {	"F.n2_2",		F.n2_2,		0xC00CCCCD	});
		tests.add(new Object[] {	"F.p100",		F.p100,		0x42C80000	});
		tests.add(new Object[] {	"F.n100",		F.n100,		0xC2C80000	});
		tests.add(new Object[] {	"F.p200",		F.p200,		0x43480000	});
		tests.add(new Object[] {	"F.n200",		F.n200,		0xC3480000	});
		tests.add(new Object[] {	"F.p1000",		F.p1000,	0x447A0000	});
		tests.add(new Object[] {	"F.n1000",		F.n1000,	0xC47A0000	});
		tests.add(new Object[] {	"F.p0_01",		F.p0_01,	0x3C23D70A	});
		tests.add(new Object[] {	"F.n0_01",		F.n0_01,	0xBC23D70A	});
		tests.add(new Object[] {	"F.p0_005",		F.p0_005,	0x3BA3D70A	});
		tests.add(new Object[] {	"F.n0_005",		F.n0_005,	0xBBA3D70A	});
		tests.add(new Object[] {	"F.p0_0001",	F.p0_0001,	0x38D1B717	});
		tests.add(new Object[] {	"F.n0_0001",	F.n0_0001,	0xB8D1B717	});
		tests.add(new Object[] {	"F.p0_0002",	F.p0_0002,	0x3951B717	});
		tests.add(new Object[] {	"F.n0_0002",	F.n0_0002,	0xB951B717	});
		tests.add(new Object[] {	"F.pC1",		F.pC1,		0x00200000	});
		tests.add(new Object[] {	"F.nC1",		F.nC1,		0x80200000	});

		tests.add(new Object[] {	"F.pBMAX",		F.pBMAX,	0x42FE0000	});
		tests.add(new Object[] {	"F.nBMAX",		F.nBMAX,	0xC3000000	});
		tests.add(new Object[] {	"F.pSMAX",		F.pSMAX,	0x46FFFE00	});
		tests.add(new Object[] {	"F.nSMAX",		F.nSMAX,	0xC7000000	});
		tests.add(new Object[] {	"F.pIMAX",		F.pIMAX,	0x4F000000	});
		tests.add(new Object[] {	"F.nIMAX",		F.nIMAX,	0xCF000000	});
		tests.add(new Object[] {	"F.pLMAX",		F.pLMAX,	0x5F000000	});
		tests.add(new Object[] {	"F.nLMAX",		F.nLMAX,	0xDF000000	});

		return tests;
	}

	public void float_constant_assumptions() {
		ArrayList<Object[]> testCases = generator();
		for (Object[] args : testCases) {
			String name = (String)args[0];
			float assumed = (float)args[1];
			long expectedLong = (int)args[2];
			float expected = F.L2F(expectedLong);
			String operation = "checking assumption " + name + " is equal to 0x" + Long.toHexString(expectedLong);
			logger.debug(operation);
			if (Float.isNaN(expected)) {
				Assert.assertTrue(Float.isNaN(assumed), operation);
			} else {
				Assert.assertEquals(assumed, expected, 0, operation);
			}
		}
	}
}
