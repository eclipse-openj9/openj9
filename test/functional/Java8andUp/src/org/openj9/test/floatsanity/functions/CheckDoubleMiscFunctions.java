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

import org.openj9.test.floatsanity.D;
import org.openj9.test.floatsanity.TD;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDoubleMiscFunctions {

	public static Logger logger = Logger.getLogger(CheckDoubleMiscFunctions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check double misc functions");
	}

	public void IEEEremainder_double() {
		ArrayList<double[]> tests = new ArrayList<>();

		tests.add(new double[] { D.NAN, D.NAN, D.NAN });
		tests.add(new double[] { D.PINF, D.NMIN, D.NAN });
		tests.add(new double[] { D.NINF, D.PMAX, D.NAN });
		tests.add(new double[] { D.PMIN, D.PZERO, D.NAN });
		tests.add(new double[] { D.NMAX, D.NZERO, D.NAN });
		tests.add(new double[] { D.PINF, D.PZERO, D.NAN });
		tests.add(new double[] { D.NINF, D.NZERO, D.NAN });
		tests.add(new double[] { D.PINF, D.NZERO, D.NAN });

		tests.add(new double[] { D.PMIN, D.PINF, D.PMIN });
		tests.add(new double[] { D.PMAX, D.NINF, D.PMAX });
		tests.add(new double[] { D.NMIN, D.PINF, D.NMIN });
		tests.add(new double[] { D.NMAX, D.NINF, D.NMAX });
		tests.add(new double[] { D.PZERO, D.PINF, D.PZERO });
		tests.add(new double[] { D.PZERO, D.NINF, D.PZERO });
		tests.add(new double[] { D.NZERO, D.PINF, D.NZERO });
		tests.add(new double[] { D.NZERO, D.NINF, D.NZERO });
		tests.add(new double[] { D.p1_1, D.PINF, D.p1_1 });
		tests.add(new double[] { D.p1_1, D.NINF, D.p1_1 });
		tests.add(new double[] { D.n1_1, D.PINF, D.n1_1 });
		tests.add(new double[] { D.n1_1, D.NINF, D.n1_1 });

		tests.add(new double[] { D.PMIN, D.PMIN, D.PZERO });
		tests.add(new double[] { D.PMIN, D.PMAX, D.PMIN });
		tests.add(new double[] { D.PMIN, D.NMIN, D.PZERO });
		tests.add(new double[] { D.PMIN, D.NMAX, D.PMIN });

		tests.add(new double[] { D.PMAX, D.PMIN, D.PZERO });
		tests.add(new double[] { D.PMAX, D.PMAX, D.PZERO });
		tests.add(new double[] { D.PMAX, D.NMIN, D.PZERO });
		tests.add(new double[] { D.PMAX, D.NMAX, D.PZERO });

		tests.add(new double[] { D.NMIN, D.NMIN, D.NZERO });
		tests.add(new double[] { D.NMIN, D.NMAX, D.NMIN });
		tests.add(new double[] { D.NMIN, D.PMIN, D.NZERO });
		tests.add(new double[] { D.NMIN, D.PMAX, D.NMIN });

		tests.add(new double[] { D.NMAX, D.NMAX, D.NZERO });
		tests.add(new double[] { D.NMAX, D.NMIN, D.NZERO });
		tests.add(new double[] { D.NMAX, D.PMIN, D.NZERO });
		tests.add(new double[] { D.NMAX, D.PMAX, D.NZERO });

		for (double[] test : tests) {
			String operation = "testing function: double Math.IEEEremainder( " + test[0] + " , " + test[1] + " ) == "
					+ test[2];
			logger.debug(operation);
			double result = Math.IEEEremainder(test[0], test[1]);
			Assert.assertEquals(result, test[2], 0, operation);
		}
	}

	public void toDegrees_double() {
		String operation;

		for (int i = 0; i < TD.radArray.length; i++) {
			operation = "testing function: double Math.toDegrees( " + TD.radArray[i] + " ) == " + TD.degArray[i];
			logger.debug(operation);
			Assert.assertEquals(Math.toDegrees(TD.radArray[i]), TD.degArray[i], 1, operation);
		}
		for (int i = 0; i < TD.nradArray.length; i++) {
			operation = "testing function: double Math.toDegrees( " + TD.nradArray[i] + " ) == " + TD.ndegArray[i];
			logger.debug(operation);
			Assert.assertEquals(Math.toDegrees(TD.nradArray[i]), TD.ndegArray[i], 1, operation);
		}
		operation = "testing function: double Math.toDegrees( " + D.NAN + " ) == " + D.NAN;
		logger.debug(operation);
		Assert.assertTrue(Double.isNaN(Math.toDegrees(D.NAN)), operation);
	}

	public void toRadians_double() {
		String operation;

		for (int i = 0; i < TD.radArray.length; i++) {
			operation = "testing function: double Math.toRadians( " + TD.degArray[i] + " ) == " + TD.radArray[i];
			logger.debug(operation);
			Assert.assertEquals(Math.toRadians(TD.degArray[i]), TD.radArray[i], 1, operation);
		}
		for (int i = 0; i < TD.nradArray.length; i++) {
			operation = "testing function: double Math.toRadians( " + TD.ndegArray[i] + " ) == " + TD.nradArray[i];
			logger.debug(operation);
			Assert.assertEquals(Math.toRadians(TD.ndegArray[i]), TD.nradArray[i], 1, operation);
		}
		operation = "testing function: double Math.toRadians( " + D.NAN + " ) == " + D.NAN;
		logger.debug(operation);
		Assert.assertTrue(Double.isNaN(Math.toRadians(D.NAN)), operation);
	}
}
