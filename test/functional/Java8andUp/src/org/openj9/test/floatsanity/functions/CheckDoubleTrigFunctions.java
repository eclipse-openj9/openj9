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
import org.openj9.test.floatsanity.TD;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDoubleTrigFunctions {

	public static Logger logger = Logger.getLogger(CheckDoubleTrigFunctions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check double trig functions");
	}

	public void sin_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < TD.sinArray.length; i++) {
			tests.add(new double[] { 1, TD.radArray[i], TD.sinArray[i] });
		}
		tests.add(new double[] { 0, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.PINF, D.NAN });
		tests.add(new double[] { 0, D.NINF, D.NAN });
		tests.add(new double[] { 0, D.PZERO, D.PZERO });
		tests.add(new double[] { 0, D.NZERO, D.NZERO });

		for (double[] test : tests) {
			String operation = "testing function: double Math.sin( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.sin(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	public void cos_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < TD.cosArray.length; i++) {
			tests.add(new double[] { 1, TD.radArray[i], TD.cosArray[i] });
		}
		tests.add(new double[] { 0, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.PINF, D.NAN });
		tests.add(new double[] { 0, D.NINF, D.NAN });
		tests.add(new double[] { 0, D.PZERO, D.pOne });
		tests.add(new double[] { 0, D.NZERO, D.pOne });

		for (double[] test : tests) {
			String operation = "testing function: double Math.cos( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.cos(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	public void tan_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < TD.tanArray.length; i++) {
			if (i == 6) {
				continue; /* tan approximation is inaccurate near PI/2 */
			}
			tests.add(new double[] { 1, TD.radArray[i], TD.tanArray[i] });
		}
		tests.add(new double[] { 0, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.PINF, D.NAN });
		tests.add(new double[] { 0, D.NINF, D.NAN });
		tests.add(new double[] { 0, D.PZERO, D.PZERO });
		tests.add(new double[] { 0, D.NZERO, D.NZERO });

		for (double[] test : tests) {
			String operation = "testing function: double Math.tan( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.tan(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	public void asin_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i <= 6; i++) {
			tests.add(new double[] { 2, TD.sinArray[i], TD.radArray[i] });
		}
		tests.add(new double[] { 0, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.PINF, D.NAN });
		tests.add(new double[] { 0, D.NINF, D.NAN });
		tests.add(new double[] { 0, D.p1_1, D.NAN });
		tests.add(new double[] { 0, D.n1_1, D.NAN });
		tests.add(new double[] { 0, D.PZERO, D.PZERO });
		tests.add(new double[] { 0, D.NZERO, D.NZERO });

		for (double[] test : tests) {
			String operation = "testing function: double Math.asin( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.asin(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	public void acos_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i <= 6; i++) {
			tests.add(new double[] { 1, TD.cosArray[i], TD.radArray[i] });
		}
		tests.add(new double[] { 0, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.PINF, D.NAN });
		tests.add(new double[] { 0, D.NINF, D.NAN });
		tests.add(new double[] { 0, D.p1_1, D.NAN });
		tests.add(new double[] { 0, D.n1_1, D.NAN });

		for (double[] test : tests) {
			String operation = "testing function: double Math.acos( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.acos(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	public void atan_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < TD.tanArray.length; i++)
			tests.add(new double[] { 1, TD.tanArray[i], TD.radArray[i] });
		tests.add(new double[] { 0, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.PZERO, D.PZERO });
		tests.add(new double[] { 0, D.NZERO, D.NZERO });
		tests.add(new double[] { 0, D.PINF, TD.PHPI });
		tests.add(new double[] { 0, D.NINF, TD.NHPI });

		for (double[] test : tests) {
			String operation = "testing function: double Math.atan( " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			double result = Math.atan(test[1]);
			if (Double.isNaN(test[2])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], test[0], operation);
			}
		}
	}

	public void atan2_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		for (int i = 0; i < TD.radArray2.length; i++) {
			tests.add(new double[] { 1, TD.sinArray[i], TD.cosArray[i], TD.radArray2[i] });
		}
		for (int i = 0; i < TD.pAxisArray.length; i++) {
			tests.add(new double[] { 0, D.PZERO, TD.pAxisArray[i], D.PZERO });
			tests.add(new double[] { 0, D.NZERO, TD.pAxisArray[i], D.NZERO });
		}
		for (int i = 0; i < TD.nAxisArray.length; i++) {
			tests.add(new double[] { 0, D.PZERO, TD.nAxisArray[i], TD.PPI });
			tests.add(new double[] { 0, D.NZERO, TD.nAxisArray[i], TD.NPI });
		}
		tests.add(new double[] { 0, D.NAN, D.pOne, D.NAN });
		tests.add(new double[] { 0, D.NAN, D.PZERO, D.NAN });
		tests.add(new double[] { 0, D.NAN, D.NZERO, D.NAN });
		tests.add(new double[] { 0, D.NAN, D.nTwo, D.NAN });
		tests.add(new double[] { 0, D.nOne, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.NZERO, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.PZERO, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.pTwo, D.NAN, D.NAN });
		tests.add(new double[] { 0, D.NAN, D.NAN, D.NAN });

		tests.add(new double[] { 0, D.PZERO, D.PMIN, D.PZERO });
		tests.add(new double[] { 0, D.PZERO, D.PMAX, D.PZERO });
		tests.add(new double[] { 0, D.PMIN, D.PINF, D.PZERO });
		tests.add(new double[] { 0, D.PMAX, D.PINF, D.PZERO });

		tests.add(new double[] { 0, D.NZERO, D.PMIN, D.NZERO });
		tests.add(new double[] { 0, D.NZERO, D.PMAX, D.NZERO });
		tests.add(new double[] { 0, D.NMIN, D.PINF, D.NZERO });
		tests.add(new double[] { 0, D.NMAX, D.PINF, D.NZERO });

		tests.add(new double[] { 0, D.PZERO, D.NMIN, TD.PPI });
		tests.add(new double[] { 0, D.PZERO, D.NMAX, TD.PPI });
		tests.add(new double[] { 0, D.PMIN, D.NINF, TD.PPI });
		tests.add(new double[] { 0, D.PMAX, D.NINF, TD.PPI });

		tests.add(new double[] { 0, D.NZERO, D.NMIN, TD.NPI });
		tests.add(new double[] { 0, D.NZERO, D.NMAX, TD.NPI });
		tests.add(new double[] { 0, D.NMIN, D.NINF, TD.NPI });
		tests.add(new double[] { 0, D.NMAX, D.NINF, TD.NPI });

		tests.add(new double[] { 0, D.PMIN, D.PZERO, TD.PHPI });
		tests.add(new double[] { 0, D.PMAX, D.PZERO, TD.PHPI });
		tests.add(new double[] { 0, D.PMIN, D.NZERO, TD.PHPI });
		tests.add(new double[] { 0, D.PMAX, D.NZERO, TD.PHPI });
		tests.add(new double[] { 0, D.PINF, D.PMIN, TD.PHPI });
		tests.add(new double[] { 0, D.PINF, D.PMAX, TD.PHPI });
		tests.add(new double[] { 0, D.PINF, D.NMIN, TD.PHPI });
		tests.add(new double[] { 0, D.PINF, D.NMAX, TD.PHPI });

		tests.add(new double[] { 0, D.NMIN, D.PZERO, TD.NHPI });
		tests.add(new double[] { 0, D.NMAX, D.PZERO, TD.NHPI });
		tests.add(new double[] { 0, D.NMIN, D.NZERO, TD.NHPI });
		tests.add(new double[] { 0, D.NMAX, D.NZERO, TD.NHPI });
		tests.add(new double[] { 0, D.NINF, D.PMIN, TD.NHPI });
		tests.add(new double[] { 0, D.NINF, D.PMAX, TD.NHPI });
		tests.add(new double[] { 0, D.NINF, D.NMIN, TD.NHPI });
		tests.add(new double[] { 0, D.NINF, D.NMAX, TD.NHPI });

		tests.add(new double[] { 0, D.PINF, D.PINF, TD.PQPI });
		tests.add(new double[] { 0, D.PINF, D.NINF, TD.P3QPI });
		tests.add(new double[] { 0, D.NINF, D.PINF, TD.NQPI });
		tests.add(new double[] { 0, D.NINF, D.NINF, TD.N3QPI });

		for (double[] test : tests) {
			String operation = "testing function: double Math.atan2( " + test[1] + " , " + test[2] + " ) == " + test[3];
			logger.debug(operation);
			double result = Math.atan2(test[1], test[2]);
			if (Double.isNaN(test[3])) {
				Assert.assertTrue(Double.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[3], test[0], operation);
			}
		}
	}

}
