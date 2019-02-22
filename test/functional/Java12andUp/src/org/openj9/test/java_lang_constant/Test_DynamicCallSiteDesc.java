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
package org.openj9.test.java_lang_constant;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.lang.constant.ClassDesc;
import java.lang.constant.ConstantDescs;
import java.lang.constant.DirectMethodHandleDesc;
import java.lang.constant.DynamicCallSiteDesc;
import java.lang.constant.MethodTypeDesc;
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.util.Optional;

/**
 * This test Java.lang.constant.DynamicCallSiteDesc API added in Java 12 and
 * later versions.
 *
 * tested methods: 
 * - resolveCallSiteDesc
 */
public class Test_DynamicCallSiteDesc {
	public static Logger logger = Logger.getLogger(Test_DynamicCallSiteDesc.class);

	private String fieldName = "test";

	private static ConstantCallSite bsm(Lookup l, String name, MethodType type) {
		// ()String
		return new ConstantCallSite(MethodHandles.constant(String.class, name));
	}

	/*
	 * Test Java 12 API DynamicCallSiteDesc.resolveCallSiteDesc()
	 */
	@Test(groups = { "level.sanity" })
	public void testDynamicCallSiteDescResolveCallSiteDesc() throws Throwable {
		/* setup */
		ClassDesc bsmOwnerDesc = Test_DynamicCallSiteDesc.class.describeConstable().orElseThrow();
		ClassDesc bsmRetTypeDesc = ConstantCallSite.class.describeConstable().orElseThrow();

		MethodTypeDesc handleType = MethodType.methodType(void.class).describeConstable().orElseThrow();

		/* describe and resolve callsite */
		DirectMethodHandleDesc bsm = ConstantDescs.ofCallsiteBootstrap(bsmOwnerDesc, "bsm", bsmRetTypeDesc);
		DynamicCallSiteDesc desc = DynamicCallSiteDesc.of(bsm, fieldName, handleType);
		CallSite resolvedSite = desc.resolveCallSiteDesc(MethodHandles.lookup());

		/* verify that the callsite contains the expected MethodHandle */
		logger.debug("testDynamicCallSiteDescResolveCallSiteDesc: resolved CallSite type is: "
				+ resolvedSite.type().toMethodDescriptorString());
		Assert.assertTrue(fieldName.equals((String)resolvedSite.getTarget().invokeExact()));

		
	}
}