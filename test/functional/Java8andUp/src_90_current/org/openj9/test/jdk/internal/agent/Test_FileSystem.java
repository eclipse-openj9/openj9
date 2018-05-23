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

import org.openj9.test.jdk.internal.agent.Test_FileSystem_Base;
import sun.management.FileSystem;
import java.io.File;
import org.testng.Assert;
import java.io.IOException;

public class Test_FileSystem extends Test_FileSystem_Base {
	
	/**
	 * @tests sun.management.FileSystem#isAccessUserOnly(java.io.File f)
	 */
	public void test_fileHasUserAccessOnly() {
		File file = testSetup();
		if (null == file) {
			return; /* skip test */
		}
		FileSystem fs = FileSystem.open();
		
		/* set write permissions for user access only */
		logger.debug("Write permissions for user access only");
		resetPermissions(file);
		file.setWritable(true, true);
		try {
			resolveTest(fs.isAccessUserOnly(file), true);
		} catch(IOException e) {
			Assert.fail();
		}
		
		/* set read permissions for user access only */
		logger.debug("Read permissions for user access only");
		resetPermissions(file);
		file.setReadable(true, true);
		try {
			resolveTest(fs.isAccessUserOnly(file), true);
		} catch(IOException e) {
			Assert.fail();
		}
		
		file.delete();
	}
	
	/**
	 * @tests sun.management.FileSystem#isAccessUserOnly(java.io.File f)
	 */
	public void test_fileHasOtherAccess() {
		File file = testSetup();
		if (null == file) {
			return; /* skip test */
		}
		FileSystem fs = FileSystem.open();
		
		/* set write permissions for all users */
		logger.debug("Write permissions set for all users.");
		resetPermissions(file);
		file.setWritable(true, false);
		try {
			resolveTest(fs.isAccessUserOnly(file), false);
		} catch(IOException e) {
			Assert.fail();
		}
		
		/* set read permissions for all users */
		logger.debug("Read permissions for all users");
		resetPermissions(file);
		file.setReadable(true, false);
		try {
			resolveTest(fs.isAccessUserOnly(file), false);
		} catch(IOException e) {
			Assert.fail();
		}
		
		file.delete();
	}
}