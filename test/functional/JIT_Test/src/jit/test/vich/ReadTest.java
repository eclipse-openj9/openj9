/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package jit.test.vich;

import java.io.FileInputStream;
import org.testng.Assert;
import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

class ReadTest {
static String fileName; 
private static Logger logger = Logger.getLogger(ReadTest.class);

@Parameters({"fileNamePara"})
@Test(groups = { "disable.generic-all" })
public void testRead(@Optional("") String fileNamePara) {
	fileName = fileNamePara;
	try {
		int ch;
		int cnt = 0;
		long time;
		logger.info("Opening file...");
		FileInputStream in = new FileInputStream(fileName);
		logger.info("File opened. Start reading...");
		long start = System.currentTimeMillis();
		while ((ch = in.read()) >= 0)
			cnt++;
		in.close();
		time = System.currentTimeMillis() - start;
		logger.info("Done reading...");
		logger.info("File length: " + cnt + " " + time + "ms");
	} catch (Exception e) {
		Assert.fail("Exception occurred");
	} 
}
}
