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

import org.openj9.test.floatsanity.D;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDoubleComparisons {

	public static Logger logger = Logger.getLogger(CheckDoubleComparisons.class);

	@BeforeClass
	public String groupName() {
		return "Check double comparisons";
	}

	double[] ordered = {
		D.NINF, D.NMAX, D.n1000, D.n200, D.n100, D.n2_2, D.nTwo, D.n1_1, D.nOne,
		D.n0_01, D.n0_005, D.n0_0001, D.nC1, D.PZERO, D.pC1, D.p0_0001, D.p0_005, D.p0_01,
		D.pOne, D.p1_1, D.pTwo, D.p2_2, D.p100,	D.p200, D.p1000, D.PMAX, D.PINF
	};

	public void double_compare_ordered_pairs() {
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

	public void double_compare_double_NaN() {
		String operation;
		for (int i = 0; i < ordered.length - 1; i++) {
			operation = "testing compare: " + ordered[i] + " < " + D.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] < D.NAN, operation);

			operation = "testing compare: " + ordered[i] + " <= " + D.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] <= D.NAN, operation);

			operation = "testing compare: " + ordered[i] + " > " + D.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] > D.NAN, operation);

			operation = "testing compare: " + ordered[i] + " >= " + D.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] >= D.NAN, operation);

			operation = "testing compare: " + ordered[i] + " == " + D.NAN;
			logger.debug(operation);
			Assert.assertFalse(ordered[i] == D.NAN, operation);

			operation = "testing compare: " + ordered[i] + " != " + D.NAN;
			logger.debug(operation);
			Assert.assertTrue(ordered[i] != D.NAN, operation);

			operation = "testing compare: " + D.NAN + " < " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(D.NAN < ordered[i], operation);

			operation = "testing compare: " + D.NAN + " <= " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(D.NAN <= ordered[i], operation);

			operation = "testing compare: " + D.NAN + " > " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(D.NAN > ordered[i], operation);

			operation = "testing compare: " + D.NAN + " >= " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(D.NAN >= ordered[i], operation);

			operation = "testing compare: " + D.NAN + " == " + ordered[i];
			logger.debug(operation);
			Assert.assertFalse(D.NAN == ordered[i], operation);

			operation = "testing compare: " + D.NAN + " != " + ordered[i];
			logger.debug(operation);
			Assert.assertTrue(D.NAN != ordered[i], operation);
		}
	}

	public void double_compare_NaNs() {
		String operation = "testing compare: " + D.NAN + " < " + D.NAN;
		logger.debug(operation);
		Assert.assertFalse(D.NAN < D.NAN, operation);

		operation = "testing compare: " + D.NAN + " <= " + D.NAN;
		logger.debug(operation);
		Assert.assertFalse(D.NAN <= D.NAN);

		operation = "testing compare: " + D.NAN + " > " + D.NAN;
		logger.debug(operation);
		Assert.assertFalse(D.NAN > D.NAN);

		operation = "testing compare: " + D.NAN + " >= " + D.NAN;
		logger.debug(operation);
		Assert.assertFalse(D.NAN >= D.NAN);

		operation = "testing compare: " + D.NAN + " == " + D.NAN;
		logger.debug(operation);
		Assert.assertFalse(D.NAN == D.NAN);

		operation = "testing compare: " + D.NAN + " != " + D.NAN;
		logger.debug(operation);
		Assert.assertTrue(D.NAN != D.NAN);
	}

	public void double_compare_zeros() {
		String operation = "testing compare: " + D.NZERO + " < " + D.PZERO;
		logger.debug(operation);
		Assert.assertFalse(D.NZERO < D.PZERO);

		operation = "testing compare: " + D.NZERO + " <= " + D.PZERO;
		logger.debug(operation);
		Assert.assertTrue(D.NZERO <= D.PZERO);

		operation = "testing compare: " + D.NZERO + " > " + D.PZERO;
		logger.debug(operation);
		Assert.assertFalse(D.NZERO > D.PZERO);

		operation = "testing compare: " + D.NZERO + " >= " + D.PZERO;
		logger.debug(operation);
		Assert.assertTrue(D.NZERO >= D.PZERO);

		operation = "testing compare: " + D.NZERO + " == " + D.PZERO;
		logger.debug(operation);
		Assert.assertTrue(D.NZERO == D.PZERO);

		operation = "testing compare: " + D.NZERO + " != " + D.PZERO;
		logger.debug(operation);
		Assert.assertFalse(D.NZERO != D.PZERO);
	}
}
