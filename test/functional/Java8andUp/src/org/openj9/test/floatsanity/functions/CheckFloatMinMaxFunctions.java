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
public class CheckFloatMinMaxFunctions {

	public static Logger logger = Logger.getLogger(CheckFloatMinMaxFunctions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check float min/max functions");
	}

	float[] ordered = {
		F.NINF, F.NMAX, F.n1000, F.n200, F.n100, F.n2_2, F.nTwo, F.n1_1, F.nOne,
		F.n0_01, F.n0_005, F.n0_0001, F.nC1, F.PZERO, F.pC1, F.p0_0001, F.p0_005, F.p0_01,
		F.pOne, F.p1_1, F.pTwo, F.p2_2, F.p100,	F.p200, F.p1000, F.PMAX, F.PINF
	};

	public void min_float_float() {
		ArrayList<float[]> tests = new ArrayList<>();

		for (int i = 0; i < ordered.length - 1; i++) {
			tests.add(new float[] { ordered[i], ordered[i + 1], ordered[i] });
			tests.add(new float[] { ordered[i + 1], ordered[i], ordered[i] });
		}
		for (int i = 0; i < ordered.length; i++) {
			tests.add(new float[] { ordered[i], F.NAN, F.NAN });
			tests.add(new float[] { F.NAN, ordered[i], F.NAN });
			tests.add(new float[] { ordered[i], ordered[i], ordered[i] });
		}
		tests.add(new float[] { F.NAN, F.NAN, F.NAN });

		tests.add(new float[] { F.NZERO, F.PZERO, F.NZERO });
		tests.add(new float[] { F.PZERO, F.NZERO, F.NZERO });
		
		for (float[] test : tests) {
			String operation = "testing function: float Math.min( " + test[0] + " , " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			float result = Math.min(test[0], test[1]);
			if (Float.isNaN(test[2])) {
				Assert.assertTrue(Float.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], 0, operation);
			}
		}
	}

	public void max_float_float() {
		ArrayList<float[]> tests = new ArrayList<>();

		for (int i = 0; i < ordered.length - 1; i++) {
			tests.add(new float[] { ordered[i], ordered[i + 1], ordered[i + 1] });
			tests.add(new float[] { ordered[i + 1], ordered[i], ordered[i + 1] });
		}
		for (int i = 0; i < ordered.length; i++) {
			tests.add(new float[] { ordered[i], F.NAN, F.NAN });
			tests.add(new float[] { F.NAN, ordered[i], F.NAN });
			tests.add(new float[] { ordered[i], ordered[i], ordered[i] });
		}
		tests.add(new float[] { F.NAN, F.NAN, F.NAN });

		tests.add(new float[] { F.NZERO, F.PZERO, F.PZERO });
		tests.add(new float[] { F.PZERO, F.NZERO, F.PZERO });

		for (float[] test : tests) {
			String operation = "testing function: float Math.max( " + test[0] + " , " + test[1] + " ) == " + test[2];
			logger.debug(operation);
			float result = Math.max(test[0], test[1]);
			if (Float.isNaN(test[2])) {
				Assert.assertTrue(Float.isNaN(result), operation);
			} else {
				Assert.assertEquals(result, test[2], 0, operation);
			}
		}
	}
}
