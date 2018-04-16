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

import org.openj9.test.floatsanity.F;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckFloatRoundingFunctions {

	public static Logger logger = Logger.getLogger(CheckFloatRoundingFunctions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check float rounding functions");
	}

	public void round_float() {
		ArrayList<Object[]> tests = new ArrayList<>();

		tests.add(new Object[] { F.PZERO, 0 });
		tests.add(new Object[] { F.NZERO, 0 });
		tests.add(new Object[] { F.pOne, 1 });
		tests.add(new Object[] { F.nOne, -1 });
		tests.add(new Object[] { F.p1_1, 1 });
		tests.add(new Object[] { F.n1_1, -1 });
		tests.add(new Object[] { F.pTwo, 2 });
		tests.add(new Object[] { F.nTwo, -2 });
		tests.add(new Object[] { F.p2_2, 2 });
		tests.add(new Object[] { F.n2_2, -2 });
		tests.add(new Object[] { F.p0_005, 0 });
		tests.add(new Object[] { F.n0_005, 0 });
		tests.add(new Object[] { F.p1000, 1000 });
		tests.add(new Object[] { F.n1000, -1000 });

		tests.add(new Object[] { F.NAN, 0 });

		for (Object[] test : tests) {
			String operation = "testing function: float Math.round( " + (float)test[0] + " ) == " + (int)test[1];
			logger.debug(operation);
			long result = Math.round((float)test[0]);
			Assert.assertEquals(result, (int)test[1], operation);
		}
	}

	public void abs_float() {
		ArrayList<float[]> tests = new ArrayList<>();
		String operation;

		tests.add(new float[] { F.PZERO, F.NZERO });
		tests.add(new float[] { F.PINF, F.NINF });
		tests.add(new float[] { F.PMAX, F.NMAX });
		tests.add(new float[] { F.PMIN, F.NMIN });

		tests.add(new float[] { F.pOne, F.nOne });
		tests.add(new float[] { F.pTwo, F.nTwo });
		tests.add(new float[] { F.p1_1, F.n1_1 });
		tests.add(new float[] { F.p2_2, F.n2_2 });
		tests.add(new float[] { F.p100, F.n100 });
		tests.add(new float[] { F.p200, F.n200 });
		tests.add(new float[] { F.p0_01, F.n0_01 });
		tests.add(new float[] { F.p1000, F.n1000 });
		tests.add(new float[] { F.p0_005, F.n0_005 });
		tests.add(new float[] { F.p0_0001, F.n0_0001 });
		tests.add(new float[] { F.p0_0002, F.n0_0002 });
		tests.add(new float[] { F.pC1, F.nC1 });

		for (float[] test : tests) {
			operation = "testing function: float Math.abs( " + test[0] + " ) == " + test[0];
			logger.debug(operation);
			float result = Math.abs(test[0]);
			Assert.assertEquals(result, test[0], 0, operation);

			operation = "testing function: float Math.abs( " + test[1] + " ) == " + test[0];
			logger.debug(operation);
			result = Math.abs(test[1]);
			Assert.assertEquals(result, test[0], 0, operation);
		}

		operation = "testing function: float Math.abs( " + F.NAN + " ) == " + F.NAN;
		logger.debug(operation);
		Assert.assertTrue(Float.isNaN(Math.abs(F.NAN)), operation);
	}
}
