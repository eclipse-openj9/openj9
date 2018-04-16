/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

package org.openj9.test.attachAPI;

import static org.openj9.test.attachAPI.SelfAttacher.ATTACH_SELF_API_SUCCEEDED_CODE;
import static org.openj9.test.attachAPI.SelfAttacher.ATTACH_SELF_IOEXCEPTION_CODE;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.fail;

import java.lang.reflect.Method;
import java.util.Arrays;

import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * This test must be invoked via testng.  Running the main method will return only the status of the attach API.
 *
 */
@Test(groups = { "level.extended" })
public class TestJava9AttachAPI  {
	protected static Logger logger = Logger.getLogger(TestJava9AttachAPI.class);
	/* attach API is disabled by default on z/OS. This enables it in child processes. */
	static final String ATTACH_ENABLE_PROPERTY = "-Dcom.ibm.tools.attach.enable=yes";
	static final String ATTACH_SELF_ENABLE_PROPERTY = "-Djdk.attach.allowAttachSelf";

	private String testName;
	@BeforeMethod(alwaysRun=true)
	protected void setUp(Method testMethod) throws Exception {
		testName = testMethod.getName(); 
		logger.debug("\n------------------------------------------------------\nstarting " + testName);
	}

	@Test
	public void testAllowAttachSelfDefault() {
		String targetClass = SelfAttacher.class.getCanonicalName();
		TargetManager target = new TargetManager(targetClass, null, Arrays.asList(new String[] {ATTACH_ENABLE_PROPERTY}), null);
		try {
			int status = target.waitFor();
			assertEquals(status, ATTACH_SELF_IOEXCEPTION_CODE, "wrong exit status");
		} catch (InterruptedException e) {
			fail("unexpected exception", e);
		}
	}
	
	@Test
	public void testAllowAttachSelfBareEnable() {
		String targetClass = SelfAttacher.class.getCanonicalName();
		TargetManager target = new TargetManager(targetClass, null, 
				Arrays.asList(new String[] {ATTACH_ENABLE_PROPERTY,  ATTACH_SELF_ENABLE_PROPERTY}), null);
		try {
			int status = target.waitFor();
			assertEquals(status, ATTACH_SELF_API_SUCCEEDED_CODE, "wrong exit status");
		} catch (InterruptedException e) {
			fail("unexpected exception", e);
		}
	}
	
	@Test
	public void testAllowAttachSelfEnable() {
		String targetClass = SelfAttacher.class.getCanonicalName();
		TargetManager target = new TargetManager(targetClass, null, 
				Arrays.asList(new String[] {ATTACH_ENABLE_PROPERTY,  ATTACH_SELF_ENABLE_PROPERTY+"=true"}), null);
		try {
			int status = target.waitFor();
			assertEquals(status, ATTACH_SELF_API_SUCCEEDED_CODE, "wrong exit status");
		} catch (InterruptedException e) {
			fail("unexpected exception", e);
		}
	}
	
	@Test
	public void testAllowAttachSelfDisable() {
		String targetClass = SelfAttacher.class.getCanonicalName();
		TargetManager target = new TargetManager(targetClass, null, 
				Arrays.asList(new String[] {ATTACH_ENABLE_PROPERTY,  ATTACH_SELF_ENABLE_PROPERTY+"=false"}), null);
		try {
			int status = target.waitFor();
			assertEquals(status, ATTACH_SELF_IOEXCEPTION_CODE, "wrong exit status");
		} catch (InterruptedException e) {
			fail("unexpected exception", e);
		}
	}
}
