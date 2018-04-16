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
package org.openj9.test.floatsanity.arithmetic;

import java.util.ArrayList;

import org.openj9.test.floatsanity.D;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDoublePlusBehaviour {

	public static Logger logger = Logger.getLogger(CheckDoublePlusBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check double plus double behaviour");
	}

	private ArrayList<double[]> generator() {
		ArrayList<double[]> tests = new ArrayList<>();

		tests.add(new double[] {	D.pOne,		D.pOne,		D.pTwo		});
		tests.add(new double[] {	D.p1_1,		D.p1_1,		D.p2_2		});
		tests.add(new double[] {	D.p100,	  	D.p100,		D.p200		});
		tests.add(new double[] {	D.p0_0001, 	D.p0_0001, 	D.p0_0002	});

		tests.add(new double[] {	D.PZERO,	D.PZERO,	D.PZERO		});
		tests.add(new double[] {	D.NZERO,	D.NZERO,	D.NZERO		});

		tests.add(new double[] {	D.PINF,		D.PINF,		D.PINF		});
		tests.add(new double[] {	D.NINF,		D.NINF,		D.NINF		});

		return tests;
	}

	private ArrayList<double[]> generator2() {
		ArrayList<double[]> tests = new ArrayList<>();

		tests.add(new double[] {	D.PINF,		D.PZERO,	D.PINF		});
		tests.add(new double[] {	D.PINF,		D.NZERO,	D.PINF		});
		tests.add(new double[] {	D.PINF,		D.pOne,		D.PINF		});

		tests.add(new double[] {	D.NINF,		D.PZERO,	D.NINF		});
		tests.add(new double[] {	D.NINF,		D.NZERO,	D.NINF		});
		tests.add(new double[] {	D.NINF,		D.pOne,		D.NINF		});

		tests.add(new double[] {	D.pOne,		D.nTwo,		D.nOne		});
		tests.add(new double[] {	D.nOne,		D.pTwo,		D.pOne		});
		tests.add(new double[] {	D.p1_1,		D.n2_2,		D.n1_1		});
		tests.add(new double[] {	D.n1_1,		D.p2_2,		D.p1_1		});
		tests.add(new double[] {	D.p0_0001,	D.n0_0002,	D.n0_0001	});
		tests.add(new double[] {	D.n0_0001,	D.p0_0002,	D.p0_0001	});
		tests.add(new double[] {	D.p100,		D.n200,		D.n100		});
		tests.add(new double[] {	D.n100,		D.p200,		D.p100		});

		return tests;
	}

	public void double_plus_double() {
		ArrayList<double[]> tests = generator();
		for (double[] test : tests) {
			double a = test[0];
			double b = test[1];
			double xr = test[2];
			String operation = a + " + " + b + " == " + xr;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(a + b, xr, 0, operation);
		}
	}

	public void double_plus_double_2() {
		ArrayList<double[]> tests = generator();
		for (double[] test : tests) {
			double a = test[0];
			double b = test[1];
			double xr = test[2];
			String operation = a + " + " + b + " == " + xr;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(a + b, xr, 0, operation);

			operation = b + " + " + a + " == " + xr;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(b + a, xr, 0, operation);
		}
	}
}
