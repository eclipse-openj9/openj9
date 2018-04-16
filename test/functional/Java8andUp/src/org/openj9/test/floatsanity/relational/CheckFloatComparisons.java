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
package org.openj9.test.floatsanity.relational;

import org.openj9.test.floatsanity.F;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckFloatComparisons {

	public static Logger logger = Logger.getLogger(CheckFloatComparisons.class);

	@BeforeClass
	public String groupName() {
		return "Check float comparisons";
	}

	float[] ordered = {
		F.NINF, F.NMAX, F.n1000, F.n200, F.n100, F.n2_2, F.nTwo, F.n1_1, F.nOne,
		F.n0_01, F.n0_005, F.n0_0001, F.nC1, F.PZERO, F.pC1, F.p0_0001, F.p0_005, F.p0_01,
		F.pOne, F.p1_1, F.pTwo, F.p2_2, F.p100,	F.p200, F.p1000, F.PMAX, F.PINF
	};

	public void float_compare_ordered_pairs() {
		String operation;
		for (int i = 0; i < ordered.length - 1; i++) {
			operation = "testing compare: " + ordered[i] + " < " + ordered[i + 1];
			logger.debug(operation);
			Assert.assertTrue(ordered[i] < ordered[i + 1], operation);

			operation = "testing compare: " + ordered[i] + " <= " + ordered[i + 1];
			logger.debug(operation);
			Assert.assertTrue(ordered[i] <= ordered[i + 1], operation);

			operation = "testing compare: " + ordered[i] + " > " + ordered[i + 1];
			logger.debug(operation);
			Assert.assertFalse(ordered[i] > ordered[i + 1], operation);

			operation = "testing compare: " + ordered[i] + " >= " + ordered[i + 1];
			logger.debug(operation);
			Assert.assertFalse(ordered[i] >= ordered[i + 1], operation);

			operation = "testing compare: " + ordered[i] + " == " + ordered[i + 1];
			logger.debug(operation);
			Assert.assertFalse(ordered[i] == ordered[i + 1], operation);

			operation = "testing compare: " + ordered[i] + " != " + ordered[i + 1];
			logger.debug(operation);
			Assert.assertTrue(ordered[i] != ordered[i + 1], operation);
		}
	}

	public void float_compare_float_NaN() {
		String operation;
		for (int i = 0; i < ordered.length - 1; i++) {
			operation = "testing compare: " + ordered[i] + " < " + F.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] < F.NAN, operation);

			operation = "testing compare: " + ordered[i] + " <= " + F.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] <= F.NAN, operation);

			operation = "testing compare: " + ordered[i] + " > " + F.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] > F.NAN, operation);

			operation = "testing compare: " + ordered[i] + " >= " + F.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] >= F.NAN, operation);

			operation = "testing compare: " + ordered[i] + " == " + F.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] == F.NAN, operation);

			operation = "testing compare: " + ordered[i] + " != " + F.NAN;
			logger.debug(operation);
			Assert.assertTrue(ordered[i] != F.NAN, operation);

			operation = "testing compare: " + F.NAN + " < " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(F.NAN < ordered[i], operation);

			operation = "testing compare: " + F.NAN + " <= " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(F.NAN <= ordered[i], operation);

			operation = "testing compare: " + F.NAN + " > " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(F.NAN > ordered[i], operation);

			operation = "testing compare: " + F.NAN + " >= " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(F.NAN >= ordered[i], operation);

			operation = "testing compare: " + F.NAN + " == " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(F.NAN == ordered[i], operation);

			operation = "testing compare: " + F.NAN + " != " + ordered[i];
			logger.debug(operation);
			Assert.assertTrue(F.NAN != ordered[i], operation);
		}
	}

	public void float_compare_NaNs() {
		String operation = "testing compare: " + F.NAN + " < " + F.NAN;
		logger.debug(operation);
		Assert.assertFalse(F.NAN < F.NAN, operation);

		operation = "testing compare: " + F.NAN + " <= " + F.NAN;
		logger.debug(operation);
		Assert.assertFalse(F.NAN <= F.NAN);

		operation = "testing compare: " + F.NAN + " > " + F.NAN;
		logger.debug(operation);
		Assert.assertFalse(F.NAN > F.NAN);

		operation = "testing compare: " + F.NAN + " >= " + F.NAN;
		logger.debug(operation);
		Assert.assertFalse(F.NAN >= F.NAN);

		operation = "testing compare: " + F.NAN + " == " + F.NAN;
		logger.debug(operation);
		Assert.assertFalse(F.NAN == F.NAN);

		operation = "testing compare: " + F.NAN + " != " + F.NAN;
		logger.debug(operation);
		Assert.assertTrue(F.NAN != F.NAN);
	}

	public void float_compare_zeros() {
		String operation = "testing compare: " + F.NZERO + " < " + F.PZERO;
		logger.debug(operation);
		Assert.assertFalse(F.NZERO < F.PZERO);

		operation = "testing compare: " + F.NZERO + " <= " + F.PZERO;
		logger.debug(operation);
		Assert.assertTrue(F.NZERO <= F.PZERO);

		operation = "testing compare: " + F.NZERO + " > " + F.PZERO;
		logger.debug(operation);
		Assert.assertFalse(F.NZERO > F.PZERO);

		operation = "testing compare: " + F.NZERO + " >= " + F.PZERO;
		logger.debug(operation);
		Assert.assertTrue(F.NZERO >= F.PZERO);

		operation = "testing compare: " + F.NZERO + " == " + F.PZERO;
		logger.debug(operation);
		Assert.assertTrue(F.NZERO == F.PZERO);

		operation = "testing compare: " + F.NZERO + " != " + F.PZERO;
		logger.debug(operation);
		Assert.assertFalse(F.NZERO != F.PZERO);
	}
}
