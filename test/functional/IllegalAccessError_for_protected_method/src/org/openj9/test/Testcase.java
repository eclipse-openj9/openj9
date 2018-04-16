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
package org.openj9.test;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * Purpose of this test is to ensure that a cross package call
 * to a protected method can succeed, even when the class doing
 * the call has been redefined.
 * 
 * SenderClass = Testcase (original class)
 * TargetClass = Testcase (redefined class)
 * DefiningClass = ClassLoader
 * 
 * We need to ensure that the sender and target classes are
 * seen as being the same class, even though they will have
 * different J9Class.
 *
 */
public class Testcase extends ClassLoader {

	public static Logger logger = Logger.getLogger(Testcase.class);

	@Test(groups = { "level.extended" })
	public void runTest() throws Throwable {
		Agent.instrumentation.retransformClasses(Testcase.class);

		try {
			byte[] bytes = null;
			defineClass("NoSuchClass", bytes, 0, 0);
			logger.error("Testcase Fail: Should have thrown exception");
		} catch (IllegalAccessError e) {
			logger.error("Testcase Fail: IllegalAccessError", e);
			return;
		} catch (ArrayIndexOutOfBoundsException e) {
			/* Successfully linked call */
		} catch (NullPointerException e) {
			/* Successfully linked call */
		}
		logger.debug("Successfully linked call");
	}
}
