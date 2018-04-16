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
package org.openj9.test.floatsanity.custom;

import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class Check1ULPMath {

	public static Logger logger = Logger.getLogger(Check1ULPMath.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check Math functions for 1ULP");
		CheckStrictMath.initializeResults();
	}

	public void ulpSinTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults1.length; i++) {
			double arg = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][0]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][1]);
			String operation = "testing 1ULP functions: double Math.sin( " + arg + " ) == " + expected;
			logger.debug(operation);
			double result = Math.sin(arg);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}

	public void ulpCosTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults1.length; i++) {
			double arg = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][0]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][2]);
			String operation = "testing 1ULP functions: double Math.cos( " + arg + " ) == " + expected;
			logger.debug(operation);
			double result = Math.cos(arg);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}

	public void ulpTanTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults1.length; i++) {
			double arg = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][0]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][3]);
			String operation = "testing 1ULP functions: double Math.tan( " + arg + " ) == " + expected;
			logger.debug(operation);
			double result = Math.tan(arg);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}

	public void ulpAsinTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults1.length; i++) {
			double arg = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][0]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][4]);
			String operation = "testing 1ULP functions: double Math.asin( " + arg + " ) == " + expected;
			logger.debug(operation);
			double result = Math.asin(arg);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}

	public void ulpAcosTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults1.length; i++) {
			double arg = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][0]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][5]);
			String operation = "testing 1ULP functions: double Math.acos( " + arg + " ) == " + expected;
			logger.debug(operation);
			double result = Math.acos(arg);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}

	public void ulpAtanTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults1.length; i++) {
			double arg = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][0]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][6]);
			String operation = "testing 1ULP functions: double Math.atan( " + arg + " ) == " + expected;
			logger.debug(operation);
			double result = Math.atan(arg);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, Math.ulp(expected), operation);
			}
		}
	}

	public void ulpExpTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults1.length; i++) {
			double arg = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][0]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][7]);
			String operation = "testing 1ULP functions: double Math.exp( " + arg + " ) == " + expected;
			logger.debug(operation);
			double result = Math.exp(arg);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}

	public void ulpLogTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults1.length; i++) {
			double arg = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][0]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults1[i][8]);
			String operation = "testing 1ULP functions: double Math.log( " + arg + " ) == " + expected;
			logger.debug(operation);
			double result = Math.log(arg);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}

	public void ulpAtan2Test() {
		for (int i = 0; i < CheckStrictMath.expectedResults2.length; i++) {
			double arg1 = Double.longBitsToDouble(CheckStrictMath.expectedResults2[i][0]);
			double arg2 = Double.longBitsToDouble(CheckStrictMath.expectedResults2[i][1]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults2[i][2]);
			String operation = "testing 1ULP functions: double Math.atan2( " + arg1 + " , " + arg2 + " ) == " + expected;
			logger.debug(operation);
			double result = Math.atan2(arg1, arg2);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}

	public void ulpPowTest() {
		for (int i = 0; i < CheckStrictMath.expectedResults2.length; i++) {
			double arg1 = Double.longBitsToDouble(CheckStrictMath.expectedResults2[i][0]);
			double arg2 = Double.longBitsToDouble(CheckStrictMath.expectedResults2[i][1]);
			double expected = Double.longBitsToDouble(CheckStrictMath.expectedResults2[i][3]);
			String operation = "testing 1ULP functions: double Math.pow( " + arg1 + " , " + arg2 + " ) == " + expected;
			logger.debug(operation);
			double result = Math.pow(arg1, arg2);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(expected), operation);
			} else {
				Assert.assertEquals(result, expected, 0, operation);
			}
		}
	}
}
