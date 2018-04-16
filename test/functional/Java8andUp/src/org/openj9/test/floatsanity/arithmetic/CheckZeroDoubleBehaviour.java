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

import org.openj9.test.floatsanity.D;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckZeroDoubleBehaviour {

	public static Logger logger = Logger.getLogger(CheckZeroDoubleBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check zero double behaviour");
	}

	public void double_zero_add() {
		String operation = D.PZERO + " + " + D.PZERO + " == " + D.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(D.PZERO + D.PZERO, D.PZERO, 0, operation);

		operation = D.NZERO + " + " + D.NZERO + " == " + D.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(D.NZERO + D.NZERO, D.NZERO, 0, operation);

		for (int i = 0; i < pValues.length; i++) {
			operation = pValues[i] + " + " + D.PZERO + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] + D.PZERO, pValues[i], 0, operation);

			operation = D.PZERO + " + " + pValues[i] + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.PZERO + pValues[i], pValues[i], 0, operation);

			operation = pValues[i] + " + " + D.NZERO + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] + D.NZERO, pValues[i], 0, operation);

			operation = D.NZERO + " + " + pValues[i] + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.NZERO + pValues[i], pValues[i], 0, operation);

			operation = nValues[i] + " + " + D.PZERO + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] + D.PZERO, nValues[i], 0, operation);

			operation = D.PZERO + " + " + nValues[i] + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.PZERO + nValues[i], nValues[i], 0, operation);

			operation = nValues[i] + " + " + D.NZERO + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] + D.NZERO, nValues[i], 0, operation);

			operation = D.NZERO + " + " + nValues[i] + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.NZERO + nValues[i], nValues[i], 0, operation);
		}
	}

	public void double_zero_minus() {
		String operation;

		for (int i = 0; i < pValues.length; i++) {
			operation = pValues[i] + " - " + D.PZERO + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] - D.PZERO, pValues[i], 0, operation);

			operation = D.PZERO + " - " + pValues[i] + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.PZERO - pValues[i], -pValues[i], 0, operation);

			operation = pValues[i] + " - " + D.NZERO + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] - D.NZERO, pValues[i], 0, operation);

			operation = D.NZERO + " - " + pValues[i] + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.NZERO - pValues[i], -pValues[i], 0, operation);

			operation = nValues[i] + " - " + D.PZERO + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] - D.PZERO, nValues[i], 0, operation);

			operation = D.PZERO + " - " + nValues[i] + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.PZERO - nValues[i], -nValues[i], 0, operation);

			operation = nValues[i] + " - " + D.NZERO + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] - D.NZERO, nValues[i], 0, operation);

			operation = D.NZERO + " - " + nValues[i] + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.NZERO - nValues[i], -nValues[i], 0, operation);
		}
	}

	public void double_zero_times_pos() {
		String operation = D.PZERO + " * " + D.PZERO + " == " + D.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(D.PZERO * D.PZERO, D.PZERO, 0);

		operation = D.NZERO + " * " + D.PZERO + " == " + D.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(D.NZERO * D.PZERO, D.NZERO, 0);

		operation = D.PZERO + " * " + D.NZERO + " == " + D.NZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(D.NZERO, D.PZERO * D.NZERO, 0);

		for (int i = 0; i < pValues.length; i++) {
			operation = D.PZERO + " * " + pValues[i] + " == " + D.PZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.PZERO * pValues[i], D.PZERO, 0);

			operation = pValues[i] + " * " + D.PZERO + " == " + D.PZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] * D.PZERO, D.PZERO, 0);

			operation = D.NZERO + " * " + pValues[i] + " == " + D.NZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.NZERO * pValues[i], D.NZERO, 0);

			operation = pValues[i] + " * " + D.NZERO + " == " + D.NZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] * D.NZERO, D.NZERO, 0);
		}
	}

	public void double_zero_times_neg() {
		String operation = D.PZERO + " * " + D.NZERO + " == " + D.NZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(D.PZERO * D.NZERO, D.NZERO, 0);

		operation = D.NZERO + " * " + D.PZERO + " == " + D.NZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(D.NZERO * D.PZERO, D.NZERO, 0);

		operation = D.NZERO + " * " + D.NZERO + " == " + D.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(D.NZERO * D.NZERO, D.PZERO, 0);

		for (int i = 0; i < pValues.length; i++) {
			operation = D.PZERO + " * " + nValues[i] + " == " + D.NZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.PZERO * nValues[i], D.NZERO, 0);

			operation = nValues[i] + " * " + D.PZERO + " == " + D.NZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] * D.PZERO, D.NZERO, 0);

			operation = D.NZERO + " * " + nValues[i] + " == " + D.PZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(D.NZERO * nValues[i], D.PZERO, 0);

			operation = nValues[i] + " * " + D.NZERO + " == " + D.PZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] * D.NZERO, D.PZERO, 0);
		}
	}

	static final double[] pValues = {
		D.PMIN, D.pC1, D.p0_0001, D.p0_01, D.pOne, D.pTwo, D.p1000, D.PMAX
	};
	static final double[] nValues = {
		D.NMIN, D.nC1, D.n0_0001, D.n0_01, D.nOne, D.nTwo, D.n1000, D.NMAX
	};
}
