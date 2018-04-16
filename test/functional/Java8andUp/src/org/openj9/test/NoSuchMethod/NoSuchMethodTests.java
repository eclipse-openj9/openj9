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
package org.openj9.test.NoSuchMethod;

import org.testng.annotations.Test;
import org.testng.annotations.DataProvider;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import java.lang.invoke.*;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.cert.Certificate;
import java.security.CodeSource;
import java.util.regex.*;
import java.util.*;
import java.util.regex.Pattern;

public class NoSuchMethodTests {

	public static final Logger logger = Logger.getLogger(NoSuchMethodTests.class);

	private static CustomClassLoader cl;// = new CustomClassLoader({new URL(".")});
	private static Pattern p = buildPattern();

	//Convenience for making code prettier
	// Used to parse exception methods
	public static Pattern buildPattern() {
		String classLoadPattern = "\\(loaded from (.+) by (.+)\\)";

		return Pattern.compile(
				"([^\\.]+)\\.([^\\)]+)(\\(.+) " + classLoadPattern + " called from (.*) " + classLoadPattern + "\\.");
	}

	@DataProvider(name = "testcases")
	public Iterator<Object[]> testCaseProvider() {
		try {
			URL[] urls = { new URL("file://.") };
			cl = new CustomClassLoader(urls);
		} catch (MalformedURLException e) {
			e.printStackTrace();
			Assert.fail("Unable to create classloader");
		}
		Iterable<TestInfo> tests = generateTestList();
		Assert.assertNotNull(tests, "Unable to generate test cases.");
		List<Object[]> testcases = new ArrayList<Object[]>();
		for (TestInfo test : tests) {
			testcases.add(new Object[] { test });
		}
		return testcases.iterator();
	}

	@Test(dataProvider = "testcases", groups = { "level.sanity" })
	public void testNoSuchMethodCalls(TestInfo info) {
		Runnable invoker = null;
		try {
			invoker = (Runnable)info.callingClass.getClazz().newInstance();
		} catch (IllegalAccessException e1) {
			e1.printStackTrace();
		} catch (InstantiationException e1) {
			e1.printStackTrace();
		}
		Assert.assertNotNull(invoker, "Caller class " + info.callingClass.getName() + " failed to instantiate.");
		try {
			logger.debug("Invoking " + info);
			invoker.run();
		} catch (NoSuchMethodError e) {
			Assert.assertTrue(verifyMsg(e.getMessage(), info), 
					String.format("Exception message should match expected for %s", info));
			return;
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
		Assert.fail("Expected a NoSuchMethod exception from " + info);
	}

	//return false for invalid message, true for valid
	private static boolean verifyMsg(String msg, TestInfo info) {
		Matcher matcher = p.matcher(msg);
		String[] expectedGroups = info.getExpectedGroups();
		logger.debug("NoSuchMethod MSG = " + msg);
		if (matcher.matches()) {
			int groupCount = matcher.groupCount();
			for (int i = 0; i < groupCount; ++i) {
				int idx = i * 2;
				String groupVal = matcher.group(i);
				logger.debug(String.format("group %d pattern- %s\n", i, matcher.group(i)));
				String expected = expectedGroups[idx + 1];
				if (expected != null) {
					if (!expected.equals(matcher.group(i))) {
						logger.error(String.format("Bad %s. Got '%s', expected '%s'\n", expectedGroups[idx], groupVal, expected));
						return false;
					}
				}
			}
			logger.debug("Test Passed");
			return true;
		}
		logger.error(String.format("Exception message fails to match correct format: %s", matcher));
		return false;
	}

	private static Iterable<TestInfo> generateTestList() {
		CodeSource codeSource;
		try {
			codeSource = new CodeSource(new URL("file://testPath.jar"), (Certificate[])null);
		} catch (MalformedURLException e) {
			e.printStackTrace();
			return null;
		}

		ArrayList<TestInfo> tests = new ArrayList<TestInfo>();
		String methodName = "doesNotExist";
		//TODO method sig can be more complex
		MethodType methodType = MethodType.methodType(int.class);
		ClassInfo appLoaderCallee = new ClassInfo(AppLoaderCallee.class);
		ClassInfo codeSourceCallee = cl.buildCallee("CodeSourceCallee", codeSource);
		ClassInfo noCodeSourceCallee = cl.buildCallee("NoCodeSourceCallee");
		ClassInfo bootstrapCallee = new ClassInfo(String.class);

		//AppLoader->AppLoader
		tests.add(new TestInfo(new ClassInfo(AppLoaderCaller1.class), appLoaderCallee, methodName, methodType));

		//AppLoader->Bootstrap
		tests.add(new TestInfo(new ClassInfo(AppLoaderCaller2.class), bootstrapCallee, methodName, methodType));

		//Custom w/o CodeSource->AppLoader
		tests.add(new TestInfo(cl.buildInvoker("CustomNoSourceInvoker1", null, appLoaderCallee, methodName, methodType),
			appLoaderCallee, methodName, methodType));

		//Custom w/o CodeSource->Custom w/o CodeSource
		tests.add(new TestInfo(cl.buildInvoker("CustomNoSourceInvoker2", null, noCodeSourceCallee, methodName, methodType),
			noCodeSourceCallee, methodName, methodType));

		//Custom w/o CodeSource->Custom w/ CodeSource
		tests.add(new TestInfo(cl.buildInvoker("CustomNoSourceInvoker3", null, codeSourceCallee, methodName, methodType),
			codeSourceCallee, methodName, methodType));

		//Custom w/o CodeSource->Bootstrap
		tests.add(new TestInfo(cl.buildInvoker("CustomNoSourceInvoker4", null, bootstrapCallee, methodName, methodType),
			bootstrapCallee, methodName, methodType));

		//Custom w/ CodeSource->AppLoader
		tests.add(new TestInfo(cl.buildInvoker("CustomWithSourceInvoker1", codeSource, appLoaderCallee, methodName, methodType),
			appLoaderCallee, methodName, methodType));

		//Custom w/ CodeSource->Custom w/o CodeSource
		tests.add(new TestInfo(cl.buildInvoker("CustomWithSourceInvoker2", codeSource, noCodeSourceCallee, methodName, methodType),
			noCodeSourceCallee, methodName, methodType));

		//Custom w/ CodeSource->Custom w/ CodeSource
		tests.add(new TestInfo(cl.buildInvoker("CustomWithSourceInvoker3", null, codeSourceCallee, methodName, methodType),
			codeSourceCallee, methodName, methodType));

		//Custom w/ CodeSource->Bootstrap
		tests.add(new TestInfo(cl.buildInvoker("CustomWithSourceInvoker4", null, bootstrapCallee, methodName, methodType),
			bootstrapCallee, methodName, methodType));

		return tests;
	}

}
