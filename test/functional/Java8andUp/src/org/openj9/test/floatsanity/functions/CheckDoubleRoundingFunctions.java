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
package org.openj9.test.floatsanity.functions;

import java.util.ArrayList;

import org.openj9.test.floatsanity.D;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDoubleRoundingFunctions {

	public static Logger logger = Logger.getLogger(CheckDoubleRoundingFunctions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check double rounding functions");
	}

	private ArrayList<double[]> generate_floor_ceil_rint() {
		ArrayList<double[]> tests = new ArrayList<>();

		tests.add(new double[] { D.PZERO, D.PZERO, D.PZERO, D.PZERO });
		tests.add(new double[] { D.NZERO, D.NZERO, D.NZERO, D.NZERO });
		tests.add(new double[] { D.pOne, D.pOne, D.pOne, D.pOne });
		tests.add(new double[] { D.nOne, D.nOne, D.nOne, D.nOne });
		tests.add(new double[] { D.p1_1, D.pOne, D.pTwo, D.pOne });
		tests.add(new double[] { D.n1_1, D.nTwo, D.nOne, D.nOne });
		tests.add(new double[] { D.p0_005, D.PZERO, D.pOne, D.PZERO });
		tests.add(new double[] { D.n0_005, D.nOne, D.NZERO, D.NZERO });

		tests.add(new double[] { D.p1000, D.p1000, D.p1000, D.p1000 });
		tests.add(new double[] { D.n1000, D.n1000, D.n1000, D.n1000 });

		tests.add(new double[] { D.NAN, D.NAN, D.NAN, D.NAN });

		tests.add(new double[] { D.p0_5, D.PZERO, D.pOne, D.PZERO });
		tests.add(new double[] { D.n0_5, D.nOne, D.NZERO, D.NZERO });

		tests.add(new double[] { D.p1_5, D.pOne, D.pTwo, D.pTwo });
		tests.add(new double[] { D.n1_5, D.nTwo, D.nOne, D.nTwo });

		tests.add(new double[] { D.PINF, D.PINF, D.PINF, D.PINF });
		tests.add(new double[] { D.NINF, D.NINF, D.NINF, D.NINF });

		tests.add(new double[] { D.PMAXODD, D.PMAXODD, D.PMAXODD, D.PMAXODD });
		tests.add(new double[] { D.NMAXODD, D.NMAXODD, D.NMAXODD, D.NMAXODD });
		tests.add(new double[] { D.PMAX, D.PMAX, D.PMAX, D.PMAX });
		tests.add(new double[] { D.NMAX, D.NMAX, D.NMAX, D.NMAX });

		return tests;
	}

	public void floor_double() {
		String operation;
		ArrayList<double[]> tests = generate_floor_ceil_rint();
		tests.add(new double[] { D.n0_005, D.nOne });

		for (double[] test : tests) {
			operation = "testing function: double Math.floor( " + test[1] + " ) == " + test[1];
			logger.debug(operation);
			double result = Math.floor(test[0]);
			Assert.assertEquals(result, test[1], 0, operation);
		}

		operation = "testing function: double Math.floor( " + D.NAN + " ) == " + D.NAN;
		logger.debug(operation);
		Assert.assertTrue(Double.isNaN(Math.floor(D.NAN)), operation);
	}

	public void ciel_double() {
		String operation;
		ArrayList<double[]> tests = generate_floor_ceil_rint();

		for (double[] test : tests) {
			operation = "testing function: double Math.ceil( " + test[2] + " ) == " + test[1];
			logger.debug(operation);
			double result = Math.ceil(test[0]);
			Assert.assertEquals(result, test[2], 0, operation);
		}

		operation = "testing function: double Math.ceil( " + D.NAN + " ) == " + D.NAN;
		logger.debug(operation);
		Assert.assertTrue(Double.isNaN(Math.ceil(D.NAN)), operation);
	}

	public void rint_double() {
		String operation;
		ArrayList<double[]> tests = generate_floor_ceil_rint();

		for (double[] test : tests) {
			operation = "testing function: double Math.rint( " + test[3] + " ) == " + test[1];
			logger.debug(operation);
			double result = Math.rint(test[0]);
			Assert.assertEquals(result, test[3], 0, operation);
		}

		operation = "testing function: double Math.rint( " + D.NAN + " ) == " + D.NAN;
		logger.debug(operation);
		Assert.assertTrue(Double.isNaN(Math.rint(D.NAN)), operation);
	}

	public void round_double() {
		ArrayList<Object[]> tests = new ArrayList<>();

		tests.add(new Object[] { D.PZERO, 0L });
		tests.add(new Object[] { D.NZERO, 0L });
		tests.add(new Object[] { D.p0_5, 1L });
		tests.add(new Object[] { D.n0_5, 0L });
		tests.add(new Object[] { D.pOne, 1L });
		tests.add(new Object[] { D.nOne, -1L });
		tests.add(new Object[] { D.p1_1, 1L });
		tests.add(new Object[] { D.n1_1, -1L });
		tests.add(new Object[] { D.p1_5, 2L });
		tests.add(new Object[] { D.n1_5, -1L });
		tests.add(new Object[] { D.pTwo, 2L });
		tests.add(new Object[] { D.nTwo, -2L });
		tests.add(new Object[] { D.p2_2, 2L });
		tests.add(new Object[] { D.n2_2, -2L });
		tests.add(new Object[] { D.p0_005, 0L });
		tests.add(new Object[] { D.n0_005, 0L });
		tests.add(new Object[] { D.p1000, 1000L });
		tests.add(new Object[] { D.n1000, -1000L });

		tests.add(new Object[] { D.NAN, 0L });

		tests.add(new Object[] { D.NINF, Long.MIN_VALUE });
		tests.add(new Object[] { D.NMAX, Long.MIN_VALUE });
		tests.add(new Object[] { (double)Long.MIN_VALUE, Long.MIN_VALUE });
		tests.add(new Object[] { D.NMIN, 0L });

		tests.add(new Object[] { D.PINF, Long.MAX_VALUE });
		tests.add(new Object[] { D.PMAX, Long.MAX_VALUE });
		tests.add(new Object[] { (double)Long.MAX_VALUE, Long.MAX_VALUE });
		tests.add(new Object[] { D.PMIN, 0L });

		for (Object[] test : tests) {
			String operation = "testing function: double Math.round( " + (double)test[0] + " ) == " + (long)test[1];
			logger.debug(operation);
			long result = Math.round((double)test[0]);
			Assert.assertEquals(result, (long)test[1], operation);
		}
	}

	public void abs_double() {
		ArrayList<double[]> tests = new ArrayList<>();
		String operation;

		tests.add(new double[] { D.PZERO, D.NZERO });
		tests.add(new double[] { D.PINF, D.NINF });
		tests.add(new double[] { D.PMAX, D.NMAX });
		tests.add(new double[] { D.PMIN, D.NMIN });

		tests.add(new double[] { D.pOne, D.nOne });
		tests.add(new double[] { D.pTwo, D.nTwo });
		tests.add(new double[] { D.p1_1, D.n1_1 });
		tests.add(new double[] { D.p2_2, D.n2_2 });
		tests.add(new double[] { D.p100, D.n100 });
		tests.add(new double[] { D.p200, D.n200 });
		tests.add(new double[] { D.p0_01, D.n0_01 });
		tests.add(new double[] { D.p1000, D.n1000 });
		tests.add(new double[] { D.p0_005, D.n0_005 });
		tests.add(new double[] { D.p0_0001, D.n0_0001 });
		tests.add(new double[] { D.p0_0002, D.n0_0002 });
		tests.add(new double[] { D.pC1, D.nC1 });

		for (double[] test : tests) {
			operation = "testing function: double Math.abs( " + test[0] + " ) == " + test[0];
			logger.debug(operation);
			double result = Math.abs(test[0]);
			Assert.assertEquals(result, test[0], 0, operation);

			operation = "testing function: double Math.abs( " + test[1] + " ) == " + test[0];
			logger.debug(operation);
			result = Math.abs(test[1]);
			Assert.assertEquals(result, test[0], 0, operation);
		}

		operation = "testing function: double Math.abs( " + D.NAN + " ) == " + D.NAN;
		logger.debug(operation);
		Assert.assertTrue(Double.isNaN(Math.abs(D.NAN)), operation);
	}
}
