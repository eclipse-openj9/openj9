/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

import openj9.tools.attach.diagnostics.info.JvmThreadInfo;
import openj9.tools.attach.diagnostics.info.ThreadGroupInfo;

@Test(groups = { "level.extended" })
public class TestJstack extends AttachApiTest {

	private static final String JSTACK_COMMAND = "jstack"; //$NON-NLS-1$
	Object syncObject = new Object();

	@Test
	public void testSingleThread() throws IOException {
		try {
			Thread testThread = new Thread(this::method1);
			testThread.start();
			while (testThread.getState() != State.WAITING) {
				try {
					Thread.sleep(1); /* sleep the minimum practical time */
				} catch (InterruptedException e) {
					/* ignore */
				}
			}
			long[] threadIds = new long[] { testThread.getId() };
			Properties props = ThreadGroupInfo.getThreadInfoProperties(threadIds).toProperties();
			logProperties(props);
			JvmThreadInfo jvmThrInfo = new JvmThreadInfo(props, 0);
			ThreadInfo thrInfo = jvmThrInfo.getThreadInfo();
			log(thrInfo.toString());
			compareThreadInfo(thrInfo, testThread);
		} finally {
			synchronized (syncObject) {
				syncObject.notifyAll();
			}
		}
	}

	@Test
	/* basic sanity: fetch and print the thread information */
	public void testAllThreads() throws IOException {
		Properties props = ThreadGroupInfo.getThreadInfoProperties().toProperties();
		logProperties(props);
		ThreadGroupInfo grpInfo = new ThreadGroupInfo(props, false);
		log(grpInfo.toString());
	}

	@Test
	public void testMonitors() throws IOException {
		synchronized (syncObject) {
			Thread testThread = new Thread(this::syncMethod, "testMonitorsThread"); //$NON-NLS-1$
			testThread.start();
			while (testThread.getState() != State.BLOCKED) {
				try {
					Thread.sleep(1); /* sleep the minimum practical time */
				} catch (InterruptedException e) {
					/* ignore */
				}
			}
			Properties props = ThreadGroupInfo.getThreadInfoProperties().toProperties();
			logProperties(props);
			ThreadGroupInfo grpInfo = new ThreadGroupInfo(props, false);
			String jstackOutput = grpInfo.toString();
			log("jstack output:\n" + jstackOutput); //$NON-NLS-1$
			assertTrue(jstackOutput.toLowerCase().contains("locked java.lang.object"), "missing 'locked' information"); //$NON-NLS-1$//$NON-NLS-2$
			assertTrue(jstackOutput.toLowerCase().contains("blocked on java.lang.object"), //$NON-NLS-1$
					"missing 'blocked' information"); //$NON-NLS-1$
		}
	}

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
		assertTrue(PlatformInfo.isOpenJ9(), "This test is valid only on OpenJ9"); //$NON-NLS-1$
		getJdkUtilityPath(JSTACK_COMMAND);
	}

	protected void compareThreadInfo(ThreadInfo thrInfo, Thread testThread) {
		assertEquals(thrInfo.getThreadId(), testThread.getId(), "wrong ID:"); //$NON-NLS-1$
		assertEquals(thrInfo.getThreadName(), testThread.getName(), "wrong name:"); //$NON-NLS-1$
		assertEquals(thrInfo.getThreadState(), testThread.getState(), "wrong state:"); //$NON-NLS-1$
		StackTraceElement[] expectedStackTrace = testThread.getStackTrace();
		StackTraceElement[] actualStackTrace = testThread.getStackTrace();
		compareStackTraces(expectedStackTrace, actualStackTrace);
	}

	protected void compareStackTraces(StackTraceElement[] expectedStackTrace, StackTraceElement[] actualStackTrace) {
		boolean match = Arrays.equals(expectedStackTrace, actualStackTrace);
		if (!match) {
			PrintStream expectedStream = StringPrintStream.factory();
			Arrays.stream(expectedStackTrace).forEach(e -> expectedStream.println(e.toString()));
			logger.error("expected:\n" + expectedStream.toString()); //$NON-NLS-1$

			PrintStream actualStream = StringPrintStream.factory();
			Arrays.stream(actualStackTrace).forEach(e -> actualStream.println(e.toString()));
			logger.error("actual:\n" + actualStream.toString()); //$NON-NLS-1$
			logger.error("Wrong stack trace"); //$NON-NLS-1$
		}
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
