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

import org.openj9.test.floatsanity.F;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckZeroFloatBehaviour {

	public static Logger logger = Logger.getLogger(CheckZeroFloatBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check zero float behaviour");
	}

	public void float_zero_add() {
		String operation = F.PZERO + " + " + F.PZERO + " == " + F.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(F.PZERO + F.PZERO, F.PZERO, 0, operation);

		operation = F.NZERO + " + " + F.NZERO + " == " + F.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(F.NZERO + F.NZERO, F.NZERO, 0, operation);

		for (int i = 0; i < pValues.length; i++) {
			operation = pValues[i] + " + " + F.PZERO + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] + F.PZERO, pValues[i], 0, operation);

			operation = F.PZERO + " + " + pValues[i] + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.PZERO + pValues[i], pValues[i], 0, operation);

			operation = pValues[i] + " + " + F.NZERO + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] + F.NZERO, pValues[i], 0, operation);

			operation = F.NZERO + " + " + pValues[i] + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.NZERO + pValues[i], pValues[i], 0, operation);

			operation = nValues[i] + " + " + F.PZERO + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] + F.PZERO, nValues[i], 0, operation);

			operation = F.PZERO + " + " + nValues[i] + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.PZERO + nValues[i], nValues[i], 0, operation);

			operation = nValues[i] + " + " + F.NZERO + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] + F.NZERO, nValues[i], 0, operation);

			operation = F.NZERO + " + " + nValues[i] + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.NZERO + nValues[i], nValues[i], 0, operation);
		}
	}

	public void float_zero_minus() {
		String operation;

		for (int i = 0; i < pValues.length; i++) {
			operation = pValues[i] + " - " + F.PZERO + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] - F.PZERO, pValues[i], 0, operation);

			operation = F.PZERO + " - " + pValues[i] + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.PZERO - pValues[i], -pValues[i], 0, operation);

			operation = pValues[i] + " - " + F.NZERO + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] - F.NZERO, pValues[i], 0, operation);

			operation = F.NZERO + " - " + pValues[i] + " == " + pValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.NZERO - pValues[i], -pValues[i], 0, operation);

			operation = nValues[i] + " - " + F.PZERO + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] - F.PZERO, nValues[i], 0, operation);

			operation = F.PZERO + " - " + nValues[i] + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.PZERO - nValues[i], -nValues[i], 0, operation);

			operation = nValues[i] + " - " + F.NZERO + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] - F.NZERO, nValues[i], 0, operation);

			operation = F.NZERO + " - " + nValues[i] + " == " + nValues[i];
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.NZERO - nValues[i], -nValues[i], 0, operation);
		}
	}

	public void float_zero_times_pos() {
		String operation = F.PZERO + " * " + F.PZERO + " == " + F.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(F.PZERO * F.PZERO, F.PZERO, 0);

		operation = F.NZERO + " * " + F.PZERO + " == " + F.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(F.NZERO * F.PZERO, F.NZERO, 0);

		operation = F.PZERO + " * " + F.NZERO + " == " + F.NZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(F.NZERO, F.PZERO * F.NZERO, 0);

		for (int i = 0; i < pValues.length; i++) {
			operation = F.PZERO + " * " + pValues[i] + " == " + F.PZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.PZERO * pValues[i], F.PZERO, 0);

			operation = pValues[i] + " * " + F.PZERO + " == " + F.PZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] * F.PZERO, F.PZERO, 0);

			operation = F.NZERO + " * " + pValues[i] + " == " + F.NZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.NZERO * pValues[i], F.NZERO, 0);

			operation = pValues[i] + " * " + F.NZERO + " == " + F.NZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(pValues[i] * F.NZERO, F.NZERO, 0);
		}
	}

	public void float_zero_times_neg() {
		String operation = F.PZERO + " * " + F.NZERO + " == " + F.NZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(F.PZERO * F.NZERO, F.NZERO, 0);

		operation = F.NZERO + " * " + F.PZERO + " == " + F.NZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(F.NZERO * F.PZERO, F.NZERO, 0);

		operation = F.NZERO + " * " + F.NZERO + " == " + F.PZERO;
		logger.debug("testing operation: " + operation);
		Assert.assertEquals(F.NZERO * F.NZERO, F.PZERO, 0);

		for (int i = 0; i < pValues.length; i++) {
			operation = F.PZERO + " * " + nValues[i] + " == " + F.NZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.PZERO * nValues[i], F.NZERO, 0);

			operation = nValues[i] + " * " + F.PZERO + " == " + F.NZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] * F.PZERO, F.NZERO, 0);

			operation = F.NZERO + " * " + nValues[i] + " == " + F.PZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(F.NZERO * nValues[i], F.PZERO, 0);

			operation = nValues[i] + " * " + F.NZERO + " == " + F.PZERO;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(nValues[i] * F.NZERO, F.PZERO, 0);
		}
	}

	static final float[] pValues = {
		F.PMIN, F.pC1, F.p0_0001, F.p0_01, F.pOne, F.pTwo, F.p1000, F.PMAX
	};
	static final float[] nValues = {
		F.NMIN, F.nC1, F.n0_0001, F.n0_01, F.nOne, F.nTwo, F.n1000, F.NMAX
	};
}
