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

import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDenormalDoubleDivBehaviour {

	public static Logger logger = Logger.getLogger(CheckDenormalDoubleDivBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check denormal double div behaviour");
	}

	public void denormal_double_div() {

		double x[] = new double[7];
		double y[] = new double[7];
		double r[] = new double[7];

		x[0] = Double.longBitsToDouble(0x001fffffffffffffL);
		y[0] = Double.longBitsToDouble(0x4000000000000000L);
		r[0] = Double.longBitsToDouble(0x0010000000000000L);

		x[1] = Double.longBitsToDouble(0x000fffffffffffffL);
		y[1] = Double.longBitsToDouble(0x7ff8000000000000L);
		r[1] = Double.longBitsToDouble(0x7ff8000000000000L);

		x[2] = Double.longBitsToDouble(0x001fffffffffffffL);
		y[2] = Double.longBitsToDouble(0xbfe0000000000000L);
		r[2] = Double.longBitsToDouble(0x802fffffffffffffL);

		x[3] = Double.longBitsToDouble(0x2000000002000000L);
		y[3] = Double.longBitsToDouble(0x5fe0000002000001L);
		r[3] = Double.longBitsToDouble(0x000fffffffffffffL);

		x[4] = Double.longBitsToDouble(0x0010000000000000L);
		y[4] = Double.longBitsToDouble(0x3ff0000000000001L);
		r[4] = Double.longBitsToDouble(0x000fffffffffffffL);

		x[5] = Double.longBitsToDouble(0x0010000000000001L);
		y[5] = Double.longBitsToDouble(0x3ff0000000000002L);
		r[5] = Double.longBitsToDouble(0x000fffffffffffffL);

		x[6] = Double.longBitsToDouble(0x02f77b3c91f02500L);
		y[6] = Double.longBitsToDouble(0x42e1dd9deee18aa0L);
		r[6] = Double.longBitsToDouble(0x000a83bed5f1643fL);

		for (int i = 0; i < x.length; i++) {
			String operation = x[i] + " / " + y[i] + " == " + r[i];
			logger.debug("testing operation: " + operation);
			if (Double.isNaN(r[i])) {
				Assert.assertTrue(Double.isNaN(x[i] / y[i]), operation);
			} else {
				Assert.assertEquals(x[i] / y[i], r[i], 0, operation);
			}
		}
	}

}
