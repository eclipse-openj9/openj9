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
public class CheckNegateDoubleBehaviour {

	public static Logger logger = Logger.getLogger(CheckNegateDoubleBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check negate double behaviour");
	}

	public void negate_double() {
		String operation = "-(" + D.NAN + ")  == " + D.NAN;
		logger.debug("testing operation: " + operation);
		Assert.assertTrue(Double.isNaN(-(D.NAN)));

		for (int i = 0; i < pValues.length; i++) {
			String operation1 = "-(" + pValues[i] + ")  == " + nValues[i];
			String operation2 = "-(" + nValues[i] + ")  == " + pValues[i];

			logger.debug("testing operation: " + operation1);
			Assert.assertEquals(-pValues[i], nValues[i], operation1);

			logger.debug("testing operation: " + operation2);
			Assert.assertEquals(-nValues[i], pValues[i], operation2);
		}
	}

	static final double[] pValues = {
		D.PZERO, D.PMIN, D.pC1, D.p0_0001, D.p0_0002, D.p0_005, D.p0_01, D.pOne, D.p1_1, D.pTwo,
		D.p2_2, D.p100, D.p200, D.p1000, D.PMAX, D.PINF
	};
	static final double[] nValues = {
		D.NZERO, D.NMIN, D.nC1, D.n0_0001, D.n0_0002, D.n0_005, D.n0_01, D.nOne, D.n1_1, D.nTwo,
		D.n2_2, D.n100, D.n200, D.n1000, D.NMAX, D.NINF
	};

}
