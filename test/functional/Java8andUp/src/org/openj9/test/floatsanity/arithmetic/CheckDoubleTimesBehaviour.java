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
public class CheckDoubleTimesBehaviour {

	public static Logger logger = Logger.getLogger(CheckDoubleTimesBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check double times double behaviour");
	}

	private ArrayList<double[]> generator() {
		ArrayList<double[]> tests = new ArrayList<>();

		tests.add(new double[] {	D.PZERO,	D.PZERO,	D.PZERO		});
		tests.add(new double[] {	D.PZERO,	D.NZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.PZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.NZERO,	D.PZERO		});

		tests.add(new double[] {	D.pOne, 	D.pOne,		D.pOne		});
		tests.add(new double[] {	D.pOne, 	D.nOne,		D.nOne		});
		tests.add(new double[] {	D.nOne, 	D.pOne,		D.nOne		});
		tests.add(new double[] {	D.nOne, 	D.nOne,		D.pOne		});

		return tests;
	}

	private ArrayList<double[]> generator2() {
		ArrayList<double[]> tests = new ArrayList<>();

		tests.add(new double[] {	D.PZERO, 	D.PINF,		D.NAN		});
		tests.add(new double[] {	D.PZERO, 	D.NINF,		D.NAN		});
		tests.add(new double[] {	D.NZERO, 	D.PINF,		D.NAN		});
		tests.add(new double[] {	D.NZERO, 	D.NINF,		D.NAN		});

		tests.add(new double[] {	D.pTwo, 	D.pOne,		D.pTwo		});
		tests.add(new double[] {	D.pTwo, 	D.nOne,		D.nTwo		});
		tests.add(new double[] {	D.nTwo, 	D.pOne,		D.nTwo		});
		tests.add(new double[] {	D.nTwo, 	D.nOne,		D.pTwo		});

		tests.add(new double[] {	D.pTwo, 	D.p1_1,		D.p2_2		});
		tests.add(new double[] {	D.pTwo, 	D.n1_1,		D.n2_2		});
		tests.add(new double[] {	D.nTwo, 	D.p1_1,		D.n2_2		});
		tests.add(new double[] {	D.nTwo, 	D.n1_1,		D.p2_2		});

		tests.add(new double[] {	D.p0_01, 	D.p100,		D.pOne		});
		tests.add(new double[] {	D.p0_01, 	D.n100,		D.nOne		});
		tests.add(new double[] {	D.n0_01, 	D.p100,		D.nOne		});
		tests.add(new double[] {	D.n0_01, 	D.n100,		D.pOne		});

		tests.add(new double[] {	D.PMAX,	 	D.pOne,		D.PMAX		});
		tests.add(new double[] {	D.PMAX,		D.nOne,		D.NMAX		});
		tests.add(new double[] {	D.NMAX,	 	D.pOne,		D.NMAX		});
		tests.add(new double[] {	D.NMAX,		D.nOne,		D.PMAX		});

		tests.add(new double[] {	D.pC1,	 	D.pOne,		D.pC1		});
		tests.add(new double[] {	D.pC1, 		D.nOne,		D.nC1		});
		tests.add(new double[] {	D.nC1, 		D.pOne,		D.nC1		});
		tests.add(new double[] {	D.nC1, 		D.nOne,		D.pC1		});

		return tests;
	}

	public void double_times_double() {
		ArrayList<double[]> tests = generator();
		for (double[] test : tests) {
			double a = test[0];
			double b = test[1];
			double xr = test[2];
			String operation = a + " * " + b + " == " + xr;
			logger.debug("testing operation: " + operation);
			if (Double.isNaN(xr)) {
				Assert.assertTrue(Double.isNaN(a * b), operation);
			} else {
				Assert.assertEquals(a * b, xr, 0, operation);
			}
		}
	}

	public void double_times_double_2() {
		ArrayList<double[]> tests = generator();
		for (double[] test : tests) {
			double a = test[0];
			double b = test[1];
			double xr = test[2];
			String operation1 = a + " * " + b + " == " + xr;
			String operation2 = b + " * " + a + " == " + xr;
			if (Double.isNaN(xr)) {
				logger.debug("testing operation: " + operation1);
				Assert.assertTrue(Double.isNaN(a * b), operation1);
				logger.debug("testing operation: " + operation2);
				Assert.assertTrue(Double.isNaN(b * a), operation2);
			} else {
				logger.debug("testing operation: " + operation1);
				Assert.assertEquals(a * b, xr, 0, operation1);
				logger.debug("testing operation: " + operation2);
				Assert.assertEquals(b * a, xr, 0, operation2);
			}
		}
	}

}
