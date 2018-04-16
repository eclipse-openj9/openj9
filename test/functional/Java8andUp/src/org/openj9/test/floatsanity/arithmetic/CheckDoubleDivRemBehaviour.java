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

import org.openj9.test.floatsanity.D;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDoubleDivRemBehaviour {

	public static Logger logger = Logger.getLogger(CheckDoubleDivRemBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check double div/rem double behaviour");
	}

	private ArrayList<double[]> generator() {
		ArrayList<double[]> tests = new ArrayList<>();
		
		tests.add(new double[] {	D.PZERO,	D.PZERO,	D.NAN,		D.NAN		});
		tests.add(new double[] {	D.PZERO,	D.NZERO,	D.NAN,		D.NAN		});
		tests.add(new double[] {	D.PZERO,	D.pOne,		D.PZERO,	D.PZERO		});
		tests.add(new double[] {	D.PZERO,	D.nOne,		D.NZERO,	D.PZERO		});
		tests.add(new double[] {	D.PZERO,	D.p200,		D.PZERO,	D.PZERO		});
		tests.add(new double[] {	D.PZERO,	D.n200,		D.NZERO,	D.PZERO		});
		tests.add(new double[] {	D.PZERO,	D.PMAX,		D.PZERO,	D.PZERO		});
		tests.add(new double[] {	D.PZERO,	D.NMAX,		D.NZERO,	D.PZERO		});
		tests.add(new double[] {	D.PZERO,	D.PINF,		D.PZERO,	D.PZERO		});
		tests.add(new double[] {	D.PZERO,	D.NINF,		D.NZERO,	D.PZERO		});

		tests.add(new double[] {	D.NZERO,	D.PZERO,	D.NAN,		D.NAN		});
		tests.add(new double[] {	D.NZERO,	D.NZERO,	D.NAN,		D.NAN		});
		tests.add(new double[] {	D.NZERO,	D.pOne,		D.NZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.nOne,		D.PZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.p200,		D.NZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.n200,		D.PZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.PMAX,		D.NZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.NMAX,		D.PZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.PINF,		D.NZERO,	D.NZERO		});
		tests.add(new double[] {	D.NZERO,	D.NINF,		D.PZERO,	D.NZERO		});

		tests.add(new double[] {	D.pOne,		D.PZERO,	D.PINF,		D.NAN		});
		tests.add(new double[] {	D.pOne,		D.NZERO,	D.NINF,		D.NAN		});
		tests.add(new double[] {	D.pOne,		D.pOne,		D.pOne,		D.PZERO		});
		tests.add(new double[] {	D.pOne,		D.nOne,		D.nOne,		D.PZERO		});
		tests.add(new double[] {	D.pOne,		D.p200,		D.p0_005,	D.pOne		});
		tests.add(new double[] {	D.pOne,		D.n200,		D.n0_005,	D.pOne		});
		tests.add(new double[] {	D.pOne,		D.PMAX, 	D.pC1, 		D.pOne		});
		tests.add(new double[] {	D.pOne,		D.NMAX, 	D.nC1,		D.pOne		});
		tests.add(new double[] {	D.pOne,		D.PINF,		D.PZERO,	D.pOne		});
		tests.add(new double[] {	D.pOne,		D.NINF,		D.NZERO,	D.pOne		});

		tests.add(new double[] {	D.nOne,		D.PZERO,	D.NINF,		D.NAN		});
		tests.add(new double[] {	D.nOne,		D.NZERO,	D.PINF,		D.NAN		});
		tests.add(new double[] {	D.nOne,		D.pOne,		D.nOne,		D.NZERO		});
		tests.add(new double[] {	D.nOne,		D.nOne,		D.pOne,		D.NZERO		});
		tests.add(new double[] {	D.nOne,		D.p200,		D.n0_005,	D.nOne		});
		tests.add(new double[] {	D.nOne,		D.n200,		D.p0_005,	D.nOne		});
		tests.add(new double[] {	D.nOne,		D.PMAX,		D.nC1,		D.nOne		});
		tests.add(new double[] {	D.nOne,		D.NMAX,		D.pC1,		D.nOne		});
		tests.add(new double[] {	D.nOne,		D.PINF,		D.NZERO,	D.nOne		});
		tests.add(new double[] {	D.nOne,		D.NINF,		D.PZERO,	D.nOne		});

		return tests;
	}

	public void double_divide() {
		ArrayList<double[]> tests = generator();
		for (double[] test : tests) {
			double a = test[0];
			double b = test[1];
			double quot = test[2];
			String operation = a + " / " + b + " == " + quot;
			logger.debug("testing operation: " + operation);
			if (Double.isNaN(quot)) {
				Assert.assertTrue(Double.isNaN(a / b), operation);
			} else {
				Assert.assertEquals(a / b, quot, 0, operation);
			}
		}
	}

	public void double_remainder() {
		ArrayList<double[]> tests = generator();
		for (double[] test : tests) {
			double a = test[0];
			double b = test[1];
			double rem = test[3];
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
