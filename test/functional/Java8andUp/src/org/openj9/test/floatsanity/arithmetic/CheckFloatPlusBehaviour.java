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
public class CheckFloatPlusBehaviour {

	public static Logger logger = Logger.getLogger(CheckFloatPlusBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check float plus float behaviour");
	}

	private ArrayList<float[]> generator() {
		ArrayList<float[]> tests = new ArrayList<>();

		tests.add(new float[] {	F.pOne,		F.pOne,		F.pTwo		});
		tests.add(new float[] {	F.p1_1,		F.p1_1,		F.p2_2		});
		tests.add(new float[] {	F.p100,	  	F.p100,		F.p200		});
		tests.add(new float[] {	F.p0_0001, 	F.p0_0001, 	F.p0_0002	});

		tests.add(new float[] {	F.PZERO,	F.PZERO,	F.PZERO		});
		tests.add(new float[] {	F.NZERO,	F.NZERO,	F.NZERO		});

		tests.add(new float[] {	F.PINF,		F.PINF,		F.PINF		});
		tests.add(new float[] {	F.NINF,		F.NINF,		F.NINF		});

		return tests;
	}

	private ArrayList<float[]> generator2() {
		ArrayList<float[]> tests = new ArrayList<>();

		tests.add(new float[] {	F.PINF,		F.PZERO,	F.PINF		});
		tests.add(new float[] {	F.PINF,		F.NZERO,	F.PINF		});
		tests.add(new float[] {	F.PINF,		F.pOne,		F.PINF		});

		tests.add(new float[] {	F.NINF,		F.PZERO,	F.NINF		});
		tests.add(new float[] {	F.NINF,		F.NZERO,	F.NINF		});
		tests.add(new float[] {	F.NINF,		F.pOne,		F.NINF		});

		tests.add(new float[] {	F.pOne,		F.nTwo,		F.nOne		});
		tests.add(new float[] {	F.nOne,		F.pTwo,		F.pOne		});
		tests.add(new float[] {	F.p1_1,		F.n2_2,		F.n1_1		});
		tests.add(new float[] {	F.n1_1,		F.p2_2,		F.p1_1		});
		tests.add(new float[] {	F.p0_0001,	F.n0_0002,	F.n0_0001	});
		tests.add(new float[] {	F.n0_0001,	F.p0_0002,	F.p0_0001	});
		tests.add(new float[] {	F.p100,		F.n200,		F.n100		});
		tests.add(new float[] {	F.n100,		F.p200,		F.p100		});

		return tests;
	}

	public void float_plus_float() {
		ArrayList<float[]> tests = generator();
		for (float[] test : tests) {
			float a = test[0];
			float b = test[1];
			float xr = test[2];
			String operation = a + " + " + b + " == " + xr;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(a + b, xr, 0, operation);
		}
	}

	public void float_plus_float_2() {
		ArrayList<float[]> tests = generator();
		for (float[] test : tests) {
			float a = test[0];
			float b = test[1];
			float xr = test[2];
			String operation = a + " + " + b + " == " + xr;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(a + b, xr, 0, operation);

			operation = b + " + " + a + " == " + xr;
			logger.debug("testing operation: " + operation);
			Assert.assertEquals(b + a, xr, 0, operation);
		}
	}
}
