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
public class CheckDenormalFloatTimesBehaviour {

	public static Logger logger = Logger.getLogger(CheckDenormalFloatTimesBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check denormal float times double behaviour");
	}

	public void denormal_float_times() {
		float x[] = new float[21];
		float y[] = new float[21];
		float r[] = new float[21];

		x[0] = Float.intBitsToFloat(0x197e03f7);
		y[0] = Float.intBitsToFloat(0x26810000);
		r[0] = Float.intBitsToFloat(0x007fffff);

		x[1] = Float.intBitsToFloat(0x00800007);
		y[1] = Float.intBitsToFloat(0x3f480000);
		r[1] = Float.intBitsToFloat(0x00640005);

		x[2] = Float.intBitsToFloat(0x00000001);
		y[2] = Float.intBitsToFloat(0x3f000000);
		r[2] = Float.intBitsToFloat(0x00000000);

		x[3] = Float.intBitsToFloat(0x3f000000);
		y[3] = Float.intBitsToFloat(0x80000001);
		r[3] = Float.intBitsToFloat(0x80000000);

		x[4] = Float.intBitsToFloat(0x007fffff);
		y[4] = Float.intBitsToFloat(0x40000000);
		r[4] = Float.intBitsToFloat(0x00fffffe);

		x[5] = Float.intBitsToFloat(0x807ffffd);
		y[5] = Float.intBitsToFloat(0xc0000000);
		r[5] = Float.intBitsToFloat(0x00fffffa);

		x[6] = Float.intBitsToFloat(0x007fffff);
		y[6] = Float.intBitsToFloat(0x7fc00000);
		r[6] = Float.intBitsToFloat(0x7fc00000);

		x[7] = Float.intBitsToFloat(0x00ffffff);
		y[7] = Float.intBitsToFloat(0x3f000000);
		r[7] = Float.intBitsToFloat(0x00800000);

		x[8] = Float.intBitsToFloat(0x7f810000);
		y[8] = Float.intBitsToFloat(0x7f810000);
		r[8] = Float.intBitsToFloat(0x7f910000);

		x[9] = Float.intBitsToFloat(0x00ffffff);
		y[9] = Float.intBitsToFloat(0x3f000000);
		r[9] = Float.intBitsToFloat(0x00800000);

		x[10] = Float.intBitsToFloat(0x00000c4b);
		y[10] = Float.intBitsToFloat(0x4526b800);
		r[10] = Float.intBitsToFloat(0x0080177e);

		x[11] = Float.intBitsToFloat(0x00800000);
		y[11] = Float.intBitsToFloat(0x00800000);
		r[11] = Float.intBitsToFloat(0x00000000);

		x[12] = Float.intBitsToFloat(0x00000c4b);
		y[12] = Float.intBitsToFloat(0x4526a800);
		r[12] = Float.intBitsToFloat(0x00800b34);

		x[12] = Float.intBitsToFloat(0x40400000);
		y[12] = Float.intBitsToFloat(0x00000002);
		r[12] = Float.intBitsToFloat(0x00000006);

		x[13] = Float.intBitsToFloat(0x3f800000);
		y[13] = Float.intBitsToFloat(0x80000009);
		r[13] = Float.intBitsToFloat(0x80000009);

		x[14] = Float.intBitsToFloat(0x00000001);
		y[14] = Float.intBitsToFloat(0x3f7fffff);
		r[14] = Float.intBitsToFloat(0x00000001);

		x[15] = Float.intBitsToFloat(0xc0600000);
		y[15] = Float.intBitsToFloat(0x80000001);
		r[15] = Float.intBitsToFloat(0x00000004);

		x[16] = Float.intBitsToFloat(0xbf000001);
		y[16] = Float.intBitsToFloat(0x00000001);
		r[16] = Float.intBitsToFloat(0x80000001);

		x[17] = Float.intBitsToFloat(0x3f000000);
		y[17] = Float.intBitsToFloat(0x80000001);
		r[17] = Float.intBitsToFloat(0x80000000);

		x[18] = Float.intBitsToFloat(0x3f000000);
		y[18] = Float.intBitsToFloat(0x00000001);
		r[18] = Float.intBitsToFloat(0x00000000);

		x[19] = Float.intBitsToFloat(0x00fffffe);
		y[19] = Float.intBitsToFloat(0xbf000000);
		r[19] = Float.intBitsToFloat(0x807fffff);

		x[20] = Float.intBitsToFloat(0x00000001);
		y[20] = Float.intBitsToFloat(0xbffffffe);
		r[20] = Float.intBitsToFloat(0x80000002);

		for (int i = 0; i < x.length; i++) {
			String operation = x[i] + " * " + y[i] + " == " + r[i];
			logger.debug("testing operation: " + operation);
			if (Float.isNaN(r[i])) {
				Assert.assertTrue(Float.isNaN(x[i] * y[i]), operation);
			} else {
				Assert.assertEquals(x[i] * y[i], r[i], 0, operation);
			}
		}
	}

}
