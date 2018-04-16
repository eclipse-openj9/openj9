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
public class CheckNaNFloatBehaviour {

	public static Logger logger = Logger.getLogger(CheckNaNFloatBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check NaN float behaviour");
	}

	public void float_NaN() {
		for (int i = 0; i < values.length; i++) {
			String operation = "NaN" + " + " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(F.NAN + values[i]));
			operation = values[i] + " + " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(values[i] + F.NAN));

			operation = "NaN" + " - " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(F.NAN - values[i]));
			operation = values[i] + " - " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(values[i] - F.NAN));

			operation = "NaN" + " * " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(F.NAN * values[i]));
			operation = values[i] + " * " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(values[i] * F.NAN));

			operation = "NaN" + " / " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(F.NAN / values[i]));
			operation = values[i] + " / " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(values[i] / F.NAN));

			operation = "NaN" + " % " + values[i] + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(F.NAN % values[i]));
			operation = values[i] + " % " + "NaN" + " == " + "NaN";
			logger.debug("testing operation: " + operation);
			Assert.assertTrue(Float.isNaN(values[i] % F.NAN));
		}
	}

	static final float[] values = {
		F.NINF, F.NMAX, F.n1000, F.n200, F.n2_2, F.nOne, F.n0_0001, F.nC1, F.NMIN, F.NZERO,
		F.NAN,
		F.PZERO, F.PMIN, F.pC1, F.p0_0001, F.pOne, F.p2_2, F.p200, F.p1000, F.PMAX, F.PINF
	};
}
