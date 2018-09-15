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
package org.openj9.test.condy;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

public class PrimitiveCondyTest {
	/**
	 * In all of the following test methods, methods from PrimitiveCondyMethods are called 3 times
	 * That helps testing following JIT scenarios.
	 * 1. Constant Dynamic is resolved
	 * 	With variation "-Xjit:count=1,disableAsyncCompilation,reResolve" , we make sure that method is
	 * 	atleast ran one time in interpreted mode so that Dynamic Constants are resolved and next method invocations
	 * 	tests JIT generated body with resolved Constant Dynamic seen by JIT compiler.
	 * 2. Constant Dynamic is unresolved
	 * 	With variation "-Xjit:count=0,disableAsyncCompilation", we make sure while compiling methods,
	 * 	JIT sees unresolved ConstantDynamic and it generates appropriate JIT body that resolves Constant
	 * 	Dynamic by calling JIT helper which calls appropriate Bootstrap methods. 
	 */
	@Test(groups = { "level.sanity" })
	public void testCondyNull() {
		Assert.assertNull(PrimitiveCondyMethods.condy_return_null());
		Assert.assertNull(PrimitiveCondyMethods.condy_return_null());
		Assert.assertNull(PrimitiveCondyMethods.condy_return_null());
	}

	@Test(groups = { "level.sanity" })
	public void testCondyInt() {
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_int(), 123432);
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_int(), 123432);
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_int(), 123432);
	}

	@Test(groups = { "level.sanity" })
	public void testCondyFloat() {
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_float(), 10.12F);
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_float(), 10.12F);
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_float(), 10.12F);
	}

	@Test(groups = { "level.sanity" })
	public void testCondyLong() {
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_long(), 100000000000L);
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_long(), 100000000000L);
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_long(), 100000000000L);
	}

	@Test(groups = { "level.sanity" })
	public void testCondyDouble() {
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_double(), 1111111.12D);
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_double(), 1111111.12D);
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_double(), 1111111.12D);
	}

	@Test(groups = { "level.sanity" })
	public void testCondyString() {
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_string(), "world");
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_string(), "world");
		Assert.assertEquals(PrimitiveCondyMethods.condy_return_string(), "world");
	}
}
