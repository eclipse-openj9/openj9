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
public class CheckNaNDoubleBehaviour {

	public static Logger logger = Logger.getLogger(CheckNaNDoubleBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check NaN double behaviour");
	}

	public void double_NaN() {
		for (int i = 0; i < values.length; i++) {
			String operation = "NaN" + " + " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(D.NAN + values[i]));
			operation = values[i] + " + " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(values[i] + D.NAN));

			operation = "NaN" + " - " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(D.NAN - values[i]));
			operation = values[i] + " - " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(values[i] - D.NAN));

			operation = "NaN" + " * " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(D.NAN * values[i]));
			operation = values[i] + " * " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(values[i] * D.NAN));

			operation = "NaN" + " / " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(D.NAN / values[i]));
			operation = values[i] + " / " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(values[i] / D.NAN));

			operation = "NaN" + " % " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(D.NAN % values[i]));
			operation = values[i] + " % " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Double.isNaN(values[i] % D.NAN));
		}
	}

	static final double[] values = {
		D.NINF, D.NMAX, D.n1000, D.n200, D.n2_2, D.nOne, D.n0_0001, D.nC1, D.NMIN, D.NZERO,
		D.NAN,
		D.PZERO, D.PMIN, D.pC1, D.p0_0001, D.pOne, D.p2_2, D.p200, D.p1000, D.PMAX, D.PINF
	};
}
