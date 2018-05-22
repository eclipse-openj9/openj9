package org.openj9.test.jdk.internal.agent;

/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.log4testng.Logger;
import java.io.File;

@Test(groups = { "level.sanity" })
public abstract class Test_FileSystem_Base {
	protected Logger logger = Logger.getLogger(getClass());

	protected File testSetup() {
		File file = null;
		
		/* method is not supported on windows */
		if (isWindows()) {
			logger.debug("skipping  test on Windows");
		} else {
		
			try {
				file = File.createTempFile("test", null);
				file.deleteOnExit();
			} catch (Exception e) {
				logger.debug("Exception thrown in creating file.");
				Assert.fail();
			}
		}
		
		return file;
	}
	
	protected boolean isWindows() {
		String osName = System.getProperty("os.name");
		if ((null != osName) && osName.startsWith("Windows")) {
			return true;
		}
		return false;
	}
	
	protected void resetPermissions(File file) {
		file.setReadable(false, false);
		file.setWritable(false, false);
	}
	
	protected void resolveTest(boolean actualResult, boolean expectedResult) {
		Assert.assertEquals(expectedResult, actualResult);
	}
}
