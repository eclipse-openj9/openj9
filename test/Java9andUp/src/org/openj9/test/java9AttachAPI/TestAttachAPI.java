/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *******************************************************************************/

package org.openj9.test.java9AttachAPI;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.fail;
import static org.openj9.test.java9AttachAPI.SelfAttacher.ATTACH_SELF_API_SUCCEEDED_CODE;
import static org.openj9.test.java9AttachAPI.SelfAttacher.ATTACH_SELF_IOEXCEPTION_CODE;

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
public class TestAttachAPI  {
	protected static Logger logger = Logger.getLogger(TestAttachAPI.class);
	private static boolean verbose = Boolean.getBoolean("org.openj9.test.java9AttachAPI.verbose");
	static final String ATTACH_ENABLE_PROPERTY = "-Dcom.ibm.tools.attach.enable=";
	static final String ATTACH_SELF_ENABLE_PROPERTY = "-Djdk.attach.allowAttachSelf";

	private String testName;
	@BeforeMethod(alwaysRun=true)
	protected void setUp(Method testMethod) throws Exception {
		testName = testMethod.getName(); 
		logger.debug("\n------------------------------------------------------\nstarting " + testName);
		TargetManager.verbose = verbose;
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
	public void testAllowAttachSelfDisnable() {
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
