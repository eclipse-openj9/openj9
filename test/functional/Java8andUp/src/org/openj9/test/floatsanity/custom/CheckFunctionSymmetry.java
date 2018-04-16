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
package org.openj9.test.floatsanity.custom;

import org.openj9.test.floatsanity.TD;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckFunctionSymmetry {

	public static Logger logger = Logger.getLogger(CheckFunctionSymmetry.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check function symmetry");
	}

	public void sin_double_sym() {
		String operation;
		for (int i = 0; i < TD.sinArray.length; i++) {
			operation = "testing function symmetry: double Math.sin( " + TD.radArray[i] + " ) == " 
					+ "-Math.sin( -" + TD.radArray[i] + " )";
			logger.debug(operation);
			Assert.assertEquals(Math.sin(TD.radArray[i]), -Math.sin(-TD.radArray[i]), 0, operation);

			operation = "testing function symmetry: double Math.asin( " + TD.sinArray[i] + " ) == " 
					+ "-Math.asin( -" + TD.sinArray[i] + " )";
			logger.debug(operation);
			Assert.assertEquals(Math.asin(TD.sinArray[i]), -Math.asin(-TD.sinArray[i]), 0, operation);
		}
	}

	public void cos_double_sym() {
		String operation;
		for (int i = 0; i < TD.cosArray.length; i++) {
			operation = "testing function symmetry: double Math.cos( " + TD.radArray[i] + " ) == " 
					+ "Math.cos( -" + TD.radArray[i] + " )";
			logger.debug(operation);
			Assert.assertEquals(Math.cos(TD.radArray[i]), Math.cos(-TD.radArray[i]), 0, operation);
		}
	}

	public void tan_double_sym() {
		String operation;
		for (int i = 0; i < TD.tanArray.length; i++) {
			operation = "testing function symmetry: double Math.tan( " + TD.tanArray[i] + " ) == " 
					+ "-Math.tan( -" + TD.tanArray[i] + " )";
			logger.debug(operation);
			Assert.assertEquals(Math.tan(TD.tanArray[i]), -Math.tan(-TD.tanArray[i]), 0, operation);

			operation = "testing function symmetry: double Math.atan( " + TD.tanArray[i] + " ) == " 
					+ "-Math.atan( -" + TD.tanArray[i] + " )";
			logger.debug(operation);
			Assert.assertEquals(Math.atan(TD.tanArray[i]), -Math.atan(-TD.tanArray[i]), 0, operation);
		}
	}

}
