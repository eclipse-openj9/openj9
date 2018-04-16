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

import java.util.ArrayList;

import org.openj9.test.floatsanity.F;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckFloatDivRemBehaviour {

	public static Logger logger = Logger.getLogger(CheckFloatDivRemBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check float div/rem float behaviour");
	}

	private ArrayList<float[]> generator() {
		ArrayList<float[]> tests = new ArrayList<>();

		tests.add(new float[] {	F.PZERO,	F.PZERO,	F.NAN,		F.NAN		});
		tests.add(new float[] {	F.PZERO,	F.NZERO,	F.NAN,		F.NAN		});
		tests.add(new float[] {	F.PZERO,	F.pOne,		F.PZERO,	F.PZERO		});
		tests.add(new float[] {	F.PZERO,	F.nOne,		F.NZERO,	F.PZERO		});
		tests.add(new float[] {	F.PZERO,	F.p200,		F.PZERO,	F.PZERO		});
		tests.add(new float[] {	F.PZERO,	F.n200,		F.NZERO,	F.PZERO		});
		tests.add(new float[] {	F.PZERO,	F.PMAX,		F.PZERO,	F.PZERO		});
		tests.add(new float[] {	F.PZERO,	F.NMAX,		F.NZERO,	F.PZERO		});
		tests.add(new float[] {	F.PZERO,	F.PINF,		F.PZERO,	F.PZERO		});
		tests.add(new float[] {	F.PZERO,	F.NINF,		F.NZERO,	F.PZERO		});

		tests.add(new float[] {	F.NZERO,	F.PZERO,	F.NAN,		F.NAN		});
		tests.add(new float[] {	F.NZERO,	F.NZERO,	F.NAN,		F.NAN		});
		tests.add(new float[] {	F.NZERO,	F.pOne,		F.NZERO,	F.NZERO		});
		tests.add(new float[] {	F.NZERO,	F.nOne,		F.PZERO,	F.NZERO		});
		tests.add(new float[] {	F.NZERO,	F.p200,		F.NZERO,	F.NZERO		});
		tests.add(new float[] {	F.NZERO,	F.n200,		F.PZERO,	F.NZERO		});
		tests.add(new float[] {	F.NZERO,	F.PMAX,		F.NZERO,	F.NZERO		});
		tests.add(new float[] {	F.NZERO,	F.NMAX,		F.PZERO,	F.NZERO		});
		tests.add(new float[] {	F.NZERO,	F.PINF,		F.NZERO,	F.NZERO		});
		tests.add(new float[] {	F.NZERO,	F.NINF,		F.PZERO,	F.NZERO		});

		tests.add(new float[] {	F.pOne,		F.PZERO,	F.PINF,		F.NAN		});
		tests.add(new float[] {	F.pOne,		F.NZERO,	F.NINF,		F.NAN		});
		tests.add(new float[] {	F.pOne,		F.pOne,		F.pOne,		F.PZERO		});
		tests.add(new float[] {	F.pOne,		F.nOne,		F.nOne,		F.PZERO		});
		tests.add(new float[] {	F.pOne,		F.p200,		F.p0_005,	F.pOne		});
		tests.add(new float[] {	F.pOne,		F.n200,		F.n0_005,	F.pOne		});
		tests.add(new float[] {	F.pOne,		F.PMAX,		F.pC1, 		F.pOne		});
		tests.add(new float[] {	F.pOne,		F.NMAX,		F.nC1,		F.pOne		});
		tests.add(new float[] {	F.pOne,		F.PINF,		F.PZERO,	F.pOne		});
		tests.add(new float[] {	F.pOne,		F.NINF,		F.NZERO,	F.pOne		});

		tests.add(new float[] {	F.nOne,		F.PZERO,	F.NINF,		F.NAN		});
		tests.add(new float[] {	F.nOne,		F.NZERO,	F.PINF,		F.NAN		});
		tests.add(new float[] {	F.nOne,		F.pOne,		F.nOne,		F.NZERO		});
		tests.add(new float[] {	F.nOne,		F.nOne,		F.pOne,		F.NZERO		});
		tests.add(new float[] {	F.nOne,		F.p200,		F.n0_005,	F.nOne		});
		tests.add(new float[] {	F.nOne,		F.n200,		F.p0_005,	F.nOne		});
		tests.add(new float[] {	F.nOne,		F.PMAX,		F.nC1,		F.nOne		});
		tests.add(new float[] {	F.nOne,		F.NMAX, 	F.pC1,		F.nOne		});
		tests.add(new float[] {	F.nOne,		F.PINF,		F.NZERO,	F.nOne		});
		tests.add(new float[] {	F.nOne,		F.NINF,		F.PZERO,	F.nOne		});

		return tests;
	}

	public void float_divide() {
		ArrayList<float[]> tests = generator();
		for (float[] test : tests) {
			float a = test[0];
			float b = test[1];
			float quot = test[2];
			String operation = a + " / " + b + " == " + quot;
			logger.debug("testing operation: " + operation);
			if (Double.isNaN(quot)) {
				Assert.assertTrue(Double.isNaN(a / b), operation);
			} else {
				Assert.assertEquals(a / b, quot, 0, operation);
			}
		}
	}

	public void float_remainder() {
		ArrayList<float[]> tests = generator();
		for (float[] test : tests) {
			float a = test[0];
			float b = test[1];
			float rem = test[3];
			String operation = a + " % " + b + " == " + rem;
			logger.debug("testing operation: " + operation);
			if (Double.isNaN(rem)) {
				Assert.assertTrue(Double.isNaN(a % b), operation);
			} else {
				Assert.assertEquals(a % b, rem, 0, operation);
			}
		}
	}
}
