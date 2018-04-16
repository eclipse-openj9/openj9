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
import org.openj9.test.floatsanity.ED;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDoubleExponentFunctions {

	public static Logger logger = Logger.getLogger(CheckDoubleExponentFunctions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check double exponent functions");
	}

	public void sqrt_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < ED.values.length; i++) {
			tests.add(new double[] { 0, ED.ncubes[i], D.NAN });
			tests.add(new double[] { 0, ED.squares[i], ED.values[i] });
			tests.add(new double[] { 1, ED.values[i], ED.squareroots[i] });
		}
		for (int i = 0; i < extremeValues.length; i++) {
			tests.add(new double[] { 0, extremeValues[i], extrSquareRoot[i] });
		}

		for (double[] test : tests) {
			String operation = "testing function: double Math.sqrt( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.sqrt(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	public void pow_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < ED.values.length; i++) {
			tests.add(new double[] { 0, D.PZERO, ED.values[i], D.PZERO });
			tests.add(new double[] { 0, D.NAN, ED.values[i], D.NAN });

			tests.add(new double[] { 1, ED.values[i], D.p0_5, ED.squareroots[i] });
			tests.add(new double[] { 1, ED.squares[i], D.p0_25, ED.squareroots[i] });
			tests.add(new double[] { 1, ED.cubes[i], D.pOneSixth, ED.squareroots[i] });

			tests.add(new double[] { 1, ED.squares[i], D.p0_5, ED.values[i] });
			tests.add(new double[] { 1, ED.cubes[i], D.pOneThird, ED.values[i] });

			tests.add(new double[] { 1, ED.squareroots[i], ED.p1, ED.squareroots[i] });
			tests.add(new double[] { 1, ED.squareroots[i], ED.p2, ED.values[i] });
			tests.add(new double[] { 1, ED.squareroots[i], ED.p4, ED.squares[i] });
			tests.add(new double[] { 1, ED.squareroots[i], ED.p6, ED.cubes[i] });

			tests.add(new double[] { 0, ED.values[i], ED.p2, ED.squares[i] });
			tests.add(new double[] { 0, ED.values[i], ED.p3, ED.cubes[i] });
			tests.add(new double[] { 0, ED.cubes[i], ED.p1, ED.cubes[i] });

			tests.add(new double[] { 0, ED.nvalues[i], ED.p2, ED.squares[i] });
			tests.add(new double[] { 0, ED.nvalues[i], ED.p3, ED.ncubes[i] });
			tests.add(new double[] { 0, ED.ncubes[i], ED.p1, ED.ncubes[i] });

			tests.add(new double[] { 1, ED.PE, ED.logvalues[i], ED.values[i] });
			tests.add(new double[] { 1, ED.PE, ED.values[i], ED.expvalues[i] });
		}
		for (int i = 0; i < extremeValues.length; i++) {
			tests.add(new double[] { 0, ED.p10, extremeValues[i], extrFinitePow[i] });
			tests.add(new double[] { 0, ED.n10, extremeValues[i], extrFinitePow[i] });
		}
		for (int i = 0; i < allValues.length; i++) {
			tests.add(new double[] { 0, allValues[i], D.PZERO, D.pOne });
			tests.add(new double[] { 0, allValues[i], D.NZERO, D.pOne });
			tests.add(new double[] { 0, allValues[i], D.pOne, allValues[i] });
			tests.add(new double[] { 0, allValues[i], D.NAN, D.NAN });
			tests.add(new double[] { 0, D.NAN, allValues[i], NANallPow[i] });
			tests.add(new double[] { 0, allValues[i], D.PINF, allPINFPow[i] });
			tests.add(new double[] { 0, allValues[i], D.NINF, allNINFPow[i] });
			tests.add(new double[] { 0, D.PZERO, allValues[i], PZEROallPow[i] });
			tests.add(new double[] { 0, D.NZERO, allValues[i], NZEROallPow[i] });
			tests.add(new double[] { 0, D.PINF, allValues[i], PINFallPow[i] });
			tests.add(new double[] { 0, D.NINF, allValues[i], NINFallPow[i] });

			tests.add(new double[] { 0, D.nOne, allValues[i], nOneallPow[i] });
			tests.add(new double[] { 0, D.pOne, allValues[i], pOneallPow[i] });
		}
		tests.add(new double[] { 0, D.n0_5, D.p0_5, D.NAN });
		tests.add(new double[] { 0, D.nTwo, D.p0_5, D.NAN });
		tests.add(new double[] { 0, D.n100, D.p0_5, D.NAN });

		tests.add(new double[] { 0, D.n0_5, D.n0_5, D.NAN });
		tests.add(new double[] { 0, D.nTwo, D.n0_5, D.NAN });
		tests.add(new double[] { 0, D.n100, D.n0_5, D.NAN });

		tests.add(new double[] { 0, D.n0_5, D.p2_2, D.NAN });
		tests.add(new double[] { 0, D.nTwo, D.p2_2, D.NAN });
		tests.add(new double[] { 0, D.n100, D.p2_2, D.NAN });

		tests.add(new double[] { 0, D.n0_5, D.n2_2, D.NAN });
		tests.add(new double[] { 0, D.nTwo, D.n2_2, D.NAN });
		tests.add(new double[] { 0, D.n100, D.n2_2, D.NAN });

		tests.add(new double[] { 0, D.p0_5, D.p1075, D.PZERO });
		tests.add(new double[] { 0, D.p0_5, D.n1075, D.PINF });
		tests.add(new double[] { 0, D.pTwo, D.p1075, D.PINF });
		tests.add(new double[] { 0, D.pTwo, D.n1075, D.PZERO });
		tests.add(new double[] { 0, D.pTwo, D.p2000, D.PINF });
		tests.add(new double[] { 0, D.pTwo, D.n2000, D.PZERO });
		tests.add(new double[] { 0, D.p100, D.p1000, D.PINF });
		tests.add(new double[] { 0, D.p100, D.n1000, D.PZERO });

		tests.add(new double[] { 0, D.n0_5, D.p1075, D.NZERO });
		tests.add(new double[] { 0, D.n0_5, D.p1076, D.PZERO });
		tests.add(new double[] { 0, D.n0_5, D.n1075, D.NINF });
		tests.add(new double[] { 0, D.n0_5, D.n1076, D.PINF });

		tests.add(new double[] { 0, D.nTwo, D.p1075, D.NINF });
		tests.add(new double[] { 0, D.nTwo, D.n1075, D.NZERO });
		tests.add(new double[] { 0, D.nTwo, D.p2000, D.PINF });
		tests.add(new double[] { 0, D.nTwo, D.n2000, D.PZERO });

		tests.add(new double[] { 0, D.n100, D.p1000, D.PINF });
		tests.add(new double[] { 0, D.n100, D.n1000, D.PZERO });
		tests.add(new double[] { 0, D.n100, D.p1075, D.NINF });
		tests.add(new double[] { 0, D.n100, D.n1075, D.NZERO });

		tests.add(new double[] { 0, D.n100, D.PMAXODD, D.NINF });
		tests.add(new double[] { 0, D.n100, D.NMAXODD, D.NZERO });

		tests.add(new double[] { 0, D.n100, D.PMAX, D.PINF });
		tests.add(new double[] { 0, D.n100, D.NMAX, D.PZERO });

		for (double[] test : tests) {
			String operation = "testing function: double Math.pow( " + test[1] + " , " + test[2] + " ) == " + test[3];
			logger.debug(operation);
			double result = Math.pow(test[1], test[2]);
			if (Double.isNaN(test[3])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[3], test[0], operation);
			}
		}
	}

	public void exp_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < ED.values.length; i++) {
			tests.add(new double[] { 1, ED.values[i], ED.expvalues[i] });
			tests.add(new double[] { 1, ED.logvalues[i], ED.values[i] });
		}
		for (int i = 0; i < extremeValues.length; i++) {
			tests.add(new double[] { 0, extremeValues[i], extrFinitePow[i] });
		}
		
		for (double[] test : tests) {
			String operation = "testing function: double Math.exp( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.exp(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	public void log_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < ED.values.length; i++) {
			tests.add(new double[] { 1, ED.values[i], ED.logvalues[i] });
			tests.add(new double[] { 1, ED.expvalues[i], ED.values[i] });
		}
		for (int i = 0; i < extremeValues.length; i++) {
			tests.add(new double[] { 0, extremeValues[i], extremeLog[i] });
		}

		for (double[] test : tests) {
			String operation = "testing function: double Math.log( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.log(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	static final double[] allValues = { 
			D.NINF, D.NMAX, D.NMAXODD, D.n1000, D.n999, ED.NE, D.n1_1, 			// x < -1
			D.nOne, D.n0_5, D.NMIN, D.NZERO, D.PZERO, D.PMIN, D.p0_5, D.pOne, 	//|x|<=1
			D.p1_1, ED.PE, D.p999, D.p1000, D.PMAXODD, D.PMAX, D.PINF, D.NAN };	// x > 1

		static final double[] NANallPow = { 
			D.NAN, D.NAN, D.NAN, D.NAN, D.NAN, D.NAN, D.NAN, 
			D.NAN, D.NAN,  D.NAN, D.pOne, 	D.pOne, D.NAN, D.NAN, D.NAN, 
			D.NAN, D.NAN, D.NAN, D.NAN, D.NAN, D.NAN, D.NAN, D.NAN };

		static final double[] allPINFPow = { 
			D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, 
			D.NAN, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.NAN, 
			D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.NAN};

		static final double[] allNINFPow = { 
			D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, 
			D.NAN, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.NAN, 
			D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.NAN};

		static final double[] PZEROallPow = { 
			D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, 
			D.PINF, D.PINF, D.PINF, D.pOne, D.pOne, D.PZERO, D.PZERO, D.PZERO, 
			D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.NAN};

		static final double[] NZEROallPow = { 
			D.PINF, D.PINF, D.NINF, D.PINF, D.NINF, D.PINF, D.PINF, 
			D.NINF, D.PINF, D.PINF, D.pOne, D.pOne, D.PZERO, D.PZERO, D.NZERO, 
			D.PZERO, D.PZERO, D.NZERO, D.PZERO, D.NZERO, D.PZERO, D.PZERO, D.NAN};

		static final double[] PINFallPow = { 	//flip of PZEROallPow
			D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, D.PZERO, 
			D.PZERO, D.PZERO, D.PZERO, D.pOne, D.pOne, D.PINF, D.PINF, D.PINF, 
			D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.PINF, D.NAN};

		static final double[] NINFallPow = { 	//flip of NZEROallPow
			D.PZERO, D.PZERO, D.NZERO, D.PZERO, D.NZERO, D.PZERO, D.PZERO, 
			D.NZERO, D.PZERO, D.PZERO, D.pOne, D.pOne, D.PINF, D.PINF, D.NINF, 
			D.PINF, D.PINF, D.NINF, D.PINF, D.NINF, D.PINF, D.PINF, D.NAN};

		static final double[] nOneallPow = {
			D.NAN, D.pOne, D.nOne, D.pOne, D.nOne, D.NAN, D.NAN, 
			D.nOne, D.NAN, D.NAN, D.pOne, D.pOne, D.NAN, D.NAN, D.nOne, 
			D.NAN, D.NAN, D.nOne, D.pOne, D.nOne, D.pOne, D.NAN, D.NAN};

		static final double[] pOneallPow = {
			D.NAN, D.pOne, D.pOne, D.pOne, D.pOne, D.pOne, D.pOne, 
			D.pOne, D.pOne, D.pOne, D.pOne, D.pOne, D.pOne, D.pOne, D.pOne, 
			D.pOne, D.pOne, D.pOne, D.pOne, D.pOne, D.pOne, D.NAN, D.NAN};


		static final double[] extremeValues = { D.PINF, D.PZERO, D.NZERO, D.NINF, D.NAN };
		static final double[] extrSquareRoot = { D.PINF, D.PZERO, D.NZERO, D.NAN, D.NAN };

		static final double[] extrFinitePow = { D.PINF, D.pOne, D.pOne, D.PZERO, D.NAN };

		static final double[] extremeLog = { D.PINF, D.NINF, D.NINF, D.NAN, D.NAN };

}
