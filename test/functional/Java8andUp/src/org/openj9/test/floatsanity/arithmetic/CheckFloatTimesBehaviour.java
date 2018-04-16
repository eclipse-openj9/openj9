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
public class CheckFloatTimesBehaviour {

	public static Logger logger = Logger.getLogger(CheckFloatTimesBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check float times float behaviour");
	}

	private ArrayList<float[]> generator() {
		ArrayList<float[]> tests = new ArrayList<>();

		tests.add(new float[] {		F.PZERO,	F.PZERO,	F.PZERO		});
		tests.add(new float[] {		F.PZERO,	F.NZERO,	F.NZERO		});
		tests.add(new float[] {		F.NZERO,	F.PZERO,	F.NZERO		});
		tests.add(new float[] {		F.NZERO,	F.NZERO,	F.PZERO		});

		tests.add(new float[] {		F.pOne, 	F.pOne,		F.pOne		});
		tests.add(new float[] {		F.pOne, 	F.nOne,		F.nOne		});
		tests.add(new float[] {		F.nOne, 	F.pOne,		F.nOne		});
		tests.add(new float[] {		F.nOne, 	F.nOne,		F.pOne		});
		
		return tests;
	}

	private ArrayList<float[]> generator2() {
		ArrayList<float[]> tests = new ArrayList<>();

		tests.add(new float[] {	F.PZERO, 	F.PINF,		F.NAN		});
		tests.add(new float[] {	F.PZERO, 	F.NINF,		F.NAN		});
		tests.add(new float[] {	F.NZERO, 	F.PINF,		F.NAN		});
		tests.add(new float[] {	F.NZERO, 	F.NINF,		F.NAN		});

		tests.add(new float[] {	F.pTwo, 	F.pOne,		F.pTwo		});
		tests.add(new float[] {	F.pTwo, 	F.nOne,		F.nTwo		});
		tests.add(new float[] {	F.nTwo, 	F.pOne,		F.nTwo		});
		tests.add(new float[] {	F.nTwo, 	F.nOne,		F.pTwo		});

		tests.add(new float[] {	F.pTwo, 	F.p1_1,		F.p2_2		});
		tests.add(new float[] {	F.pTwo, 	F.n1_1,		F.n2_2		});
		tests.add(new float[] {	F.nTwo, 	F.p1_1,		F.n2_2		});
		tests.add(new float[] {	F.nTwo, 	F.n1_1,		F.p2_2		});

		tests.add(new float[] {	F.p0_01, 	F.p100,		F.pOne		});
		tests.add(new float[] {	F.p0_01, 	F.n100,		F.nOne		});
		tests.add(new float[] {	F.n0_01, 	F.p100,		F.nOne		});
		tests.add(new float[] {	F.n0_01, 	F.n100,		F.pOne		});

		tests.add(new float[] {	F.PMAX,	 	F.pOne,		F.PMAX		});
		tests.add(new float[] {	F.PMAX,		F.nOne,		F.NMAX		});
		tests.add(new float[] {	F.NMAX,	 	F.pOne,		F.NMAX		});
		tests.add(new float[] {	F.NMAX,		F.nOne,		F.PMAX		});

		tests.add(new float[] {	F.pC1,	 	F.pOne,		F.pC1		});
		tests.add(new float[] {	F.pC1, 		F.nOne,		F.nC1		});
		tests.add(new float[] {	F.nC1, 		F.pOne,		F.nC1		});
		tests.add(new float[] {	F.nC1, 		F.nOne,		F.pC1		});

		return tests;
	}

	public void float_times_float() {
		ArrayList<float[]> tests = generator();
		for (float[] test : tests) {
			float a = test[0];
			float b = test[1];
			float xr = test[2];
			String operation = a + " * " + b + " == " + xr;
			logger.debug("testing operation: " + operation);
			if (Float.isNaN(xr)) {
				Assert.assertTrue(Float.isNaN(a * b), operation);
			} else {
				Assert.assertEquals(a * b, xr, 0, operation);
			}
		}
	}

	public void float_times_float_2() {
		ArrayList<float[]> tests = generator();
		for (float[] test : tests) {
			float a = test[0];
			float b = test[1];
			float xr = test[2];
			String operation1 = a + " * " + b + " == " + xr;
			String operation2 = b + " * " + a + " == " + xr;
			if (Float.isNaN(xr)) {
				logger.debug("testing operation: " + operation1);
				Assert.assertTrue(Float.isNaN(a * b), operation1);
				logger.debug("testing operation: " + operation2);
				Assert.assertTrue(Float.isNaN(b * a), operation2);
			} else {
				logger.debug("testing operation: " + operation1);
				Assert.assertEquals(a * b, xr, 0, operation1);
				logger.debug("testing operation: " + operation2);
				Assert.assertEquals(b * a, xr, 0, operation2);
			}
		}
	}
}
