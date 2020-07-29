/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;

import java.io.IOException;
import java.io.PrintStream;
import java.lang.Thread.State;
import java.lang.management.ThreadInfo;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.Properties;

import org.openj9.test.util.PlatformInfo;
import org.openj9.test.util.StringPrintStream;
import org.openj9.test.util.StringUtilities;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;

@Test(groups = { "level.extended" })
public class TestJstack extends AttachApiTest {

	private static final String JSTACK_COMMAND = "jstack"; //$NON-NLS-1$
	Object syncObject = new Object();


	@Test
	public void testRemote() throws IOException {
		String myId = TargetManager.getVmId();
		List<String> jpsOutput = runCommand(Collections.singletonList(myId));
		StringBuilder buff = new StringBuilder("jstack output:\n"); //$NON-NLS-1$
		jpsOutput.forEach(s -> {
			buff.append(s);
			buff.append("\n"); //$NON-NLS-1$
		});
		log(buff.toString());
		Optional<String> searchResult = StringUtilities.searchSubstring(testName, jpsOutput);
		assertTrue(searchResult.isPresent(), "Method name missing"); //$NON-NLS-1$
	}

	@Test
	public void testSynchronizers() throws IOException {
		String myId = TargetManager.getVmId();
		List<String> args = new ArrayList<>();
		args.add(myId);
		args.add("-l"); //$NON-NLS-1$
		List<String> jpsOutput = runCommand(args);
		StringBuilder buff = new StringBuilder("jstack output:\n"); //$NON-NLS-1$
		jpsOutput.forEach(s -> {
			buff.append(s);
			buff.append("\n"); //$NON-NLS-1$
		});
		log(buff.toString());
		Optional<String> searchResult = StringUtilities.searchSubstring(testName, jpsOutput);
		assertTrue(searchResult.isPresent(), "Method name missing"); //$NON-NLS-1$
	}

	@Test
	public void testProperties() throws IOException {
		Properties myProps = System.getProperties();
		final String TEST_VALUE = "test_property_value"; //$NON-NLS-1$
		final String TEST_PROPERTY = "test.property"; //$NON-NLS-1$
		myProps.setProperty(TEST_PROPERTY, TEST_VALUE);
		String myId = TargetManager.getVmId();
		List<String> args = new ArrayList<>();
		args.add(myId);
		args.add("-p"); //$NON-NLS-1$
		List<String> jpsOutput = runCommand(args);
		StringBuilder buff = new StringBuilder("jstack output:\n"); //$NON-NLS-1$
		jpsOutput.forEach(s -> {
			buff.append(s);
			buff.append("\n"); //$NON-NLS-1$
		});
		log(buff.toString());
		Optional<String> searchResult = StringUtilities.searchSubstring(TEST_VALUE, jpsOutput);
		assertTrue(searchResult.isPresent(), TEST_PROPERTY + " missing"); //$NON-NLS-1$
	}

	@BeforeMethod
	protected void setUp(Method testMethod) {
		testName = testMethod.getName();
		log("------------------------------------\nstarting " + testName); //$NON-NLS-1$
	}

	@BeforeSuite
	protected void setupSuite() {
		getJdkUtilityPath(JSTACK_COMMAND);
	}

	void method1() {
		method2();
	}

	void method2() {
		synchronized (syncObject) {
			try {
				log("method2: Wait"); //$NON-NLS-1$
				syncObject.wait();
				log("method2: Resume");//$NON-NLS-1$
			} catch (InterruptedException e) {
				/* ignore */
			}
		}
	}

	void syncMethod() {
		log("syncMethod: Acquire monitor");//$NON-NLS-1$
		synchronized (syncObject) {
			log("syncMethod: Acquired monitor");//$NON-NLS-1$
		}
		log("syncMethod: Release monitor");//$NON-NLS-1$
	}
}
