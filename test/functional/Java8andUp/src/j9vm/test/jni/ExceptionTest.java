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
package j9vm.test.jni;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;

@Test(groups = { "level.extended" })
public class ExceptionTest {

	private static final Logger logger = Logger.getLogger(ExceptionTest.class);


	private static final String JNI_ON_LOAD_MESSAGE = "JNI_OnLoad";
	private boolean exceptionThrown;
	String TEST_LIBRARY = "j9ben";

	/**
	 * @param args
	 */

	public void test_loadLibraryExceptions() {
		logger.debug("test131083 enter");
		System.setProperty("j9vm.test.jnitest.throw", "yes");
		logger.debug("Throw exception, return value okay");

		try {
			System.loadLibrary(TEST_LIBRARY);
		} catch (Throwable e) {
			verifyJniOnloadException(e);
			logger.debug("excThrown=" + exceptionThrown);
		}
	}

	public void test_loadLibraryFail() {
		System.setProperty("j9vm.test.jnitest.throw", "no");
		System.setProperty("j9vm.test.jnitest.fail", "yes");
		logger.debug("Do not throw exception, return value bad");
		try {
			System.loadLibrary(TEST_LIBRARY);
		} catch (Throwable e) {
			e.printStackTrace();
			//Expected java.lang.UnsatisfiedLinkError thrown
			AssertJUnit.assertTrue("Thrown exception is not java.lang.UnsatisfiedLinkError", e.getClass().equals(java.lang.UnsatisfiedLinkError.class));
			exceptionThrown = true;
		}
	}

	public void test_loadLibraryExceptionAndFail() {
		System.setProperty("j9vm.test.jnitest.throw", "yes");
		System.setProperty("j9vm.test.jnitest.fail", "yes");
		logger.debug("Throw exception, return value bad");
		try {
			System.loadLibrary(TEST_LIBRARY);
		} catch (Throwable e) {
			verifyJniOnloadException(e);
			logger.debug("excThrown=" + exceptionThrown);
		}
	}

	/* ==================== Utility functions ============================= */

	@BeforeMethod
	protected void setUp() throws Exception {
		logger.debug("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
		exceptionThrown = false;
	}

	@AfterMethod
	protected void tearDown() throws Exception {
		AssertJUnit.assertTrue("Expected and exception or error, none thrown", exceptionThrown);
		logger.debug("---------------------------------------------------------");
	}

	private void verifyJniOnloadException(Throwable e) {
		AssertJUnit.assertNotNull("exception  is null", e);
		logger.debug("verifyJniOnloadException, exception is \n");
		e.printStackTrace();
		exceptionThrown = true;

		AssertJUnit.assertTrue("Thrown exception is not java.lang.Exception", e.getClass().equals(Exception.class));
		String msg = e.getMessage();
		AssertJUnit.assertNotNull("exception message is null", msg);
		AssertJUnit.assertTrue("correct message in exception", msg.equalsIgnoreCase(JNI_ON_LOAD_MESSAGE));
	}

}
