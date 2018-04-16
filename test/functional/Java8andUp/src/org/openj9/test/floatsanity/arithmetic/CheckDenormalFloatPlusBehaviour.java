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
public class CheckDenormalFloatPlusBehaviour {

	public static Logger logger = Logger.getLogger(CheckDenormalFloatPlusBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check denormal float plus behaviour");
	}

	public void denormal_float_plus() {
		float x[] = new float[2];
		float y[] = new float[2];
		float r[] = new float[2];

		x[0] = Float.intBitsToFloat(0x01000000);
		y[0] = Float.intBitsToFloat(0x80ffffff);
		r[0] = Float.intBitsToFloat(0x00000001);

		x[1] = Float.intBitsToFloat(0x80800001);
		y[1] = Float.intBitsToFloat(0x00800000);
		r[1] = Float.intBitsToFloat(0x80000001);

		for (int i = 0; i < x.length; i++) {
			String operation = x[i] + " + " + y[i] + " == " + r[i];
			logger.debug("testing operation: " + operation);
			if (Float.isNaN(r[i])) {
				Assert.assertTrue(Float.isNaN(x[i] + y[i]), operation);
			} else {
				Assert.assertEquals(x[i] + y[i], r[i], 0, operation);
			}
		}
	}

}
