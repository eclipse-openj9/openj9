/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
package org.openj9.test.java_lang;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.util.Optional;

/**
 * This test Java.lang.String API added in Java 12 and later version.
 *
 */
public class Test_String {
	public static Logger logger = Logger.getLogger(Test_String.class);

	private String empty = "";
	private String latin1 = "abc123";
	private String nonLatin1 = "abc\u0153";
	private String emptyWithTerm = "\n";

	/*
	 * Test Java 12 API String.describeConstable()
	 */
	@Test(groups = { "level.sanity" })
	public void testStringDescribeConstable() throws Throwable {
		testStringDescribeConstable_sub(empty);
		testStringDescribeConstable_sub(latin1);
		testStringDescribeConstable_sub(nonLatin1);
		testStringDescribeConstable_sub(emptyWithTerm);
	}

	private void testStringDescribeConstable_sub(String test) throws Throwable {
		logger.debug("testStringDescribeConstable: test with string: " + test);
		Optional<String> optionalTest = test.describeConstable();
		String describedTestString = optionalTest.orElseThrow();
		Assert.assertTrue(test.equals(describedTestString));
	}

	/*
	 * Test Java 12 API String.resolveConstantDesc()
	 */
	@Test(groups = { "level.sanity" })
	public void testStringResolveConstantDesc() {
		testStringResolveConstantDesc_sub(empty);
		testStringResolveConstantDesc_sub(latin1);
		testStringResolveConstantDesc_sub(nonLatin1);
		testStringResolveConstantDesc_sub(emptyWithTerm);
	}

	private void testStringResolveConstantDesc_sub(String test) {
		logger.debug("testStringDescribeConstable: test with string: " + test);

		/* run test with a valid lookup */
		MethodHandles.Lookup lookup = MethodHandles.publicLookup();
		String resolvedTest = test.resolveConstantDesc(lookup);
		Assert.assertTrue(test.equals(resolvedTest));

		/* run tests with a null lookup (should be ignored) */
		String resolvedTest2 = test.resolveConstantDesc(null);
		Assert.assertTrue(test.equals(resolvedTest2));
	}
}
