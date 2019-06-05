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
package org.openj9.test.java_lang_invoke;

import java.lang.constant.MethodTypeDesc;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.util.Optional;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * This test Java.lang.invoke.MethodType API added in Java 12 and later version.
 *
 * new methods:
 * - descriptorString: returns methodDescriptor, not tested
 * - describeConstable
 */
public class Test_MethodType {
    public static Logger logger = Logger.getLogger(Test_MethodType.class);

	/*
	 * Test Java 12 API MethodType.describeConstable()
	 */
	@Test(groups = { "level.sanity" })
	public void testMethodTypeDescribeConstable() throws Throwable {
		describeConstableTestGeneral("testMethodTypeDescribeConstable", MethodType.methodType(void.class));
		describeConstableTestGeneral("testMethodTypeDescribeConstable", MethodType.methodType(String.class));
		describeConstableTestGeneral("testMethodTypeDescribeConstable", MethodType.methodType(String.class, int.class));
	}

	private void describeConstableTestGeneral(String testName, MethodType type) throws Throwable {
		Optional<MethodTypeDesc> descOp = type.describeConstable();

		MethodTypeDesc desc = descOp.orElseThrow();

		/* verify that descriptor can be resolved. Otherwise a ReflectiveOperationException will be thrown */
		MethodType resolveType = (MethodType)desc.resolveConstantDesc(MethodHandles.lookup());

		logger.debug(testName + ": Descriptor of original type is: " + type.descriptorString() + " descriptor of descriptor is: " + resolveType.descriptorString());
		Assert.assertTrue(type.equals(resolveType));
	}
}