/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package j9vm.test.jni;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity" })
public class PthreadTest {

	private static final Logger logger = Logger.getLogger(PthreadTest.class);

	private static native boolean attachAndDetach();

	private static boolean worked = false;


	public void test_destructor() {
		worked = attachAndDetach();
	}

	@BeforeMethod
	protected void setUp() throws Exception {
		logger.debug("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
		logger.debug("On an unfixed VM, this test will hang");
		System.loadLibrary("j9ben");
	}

	@AfterMethod
	protected void tearDown() throws Exception {
		AssertJUnit.assertTrue("Thread successfully detached", worked);
		logger.debug("---------------------------------------------------------");
	}

}
