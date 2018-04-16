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
package org.openj9.test.stackWalker;

import static org.testng.Assert.assertTrue;
import static org.testng.Assert.fail;

import java.lang.StackWalker.Option;
import java.util.Arrays;
import java.util.HashSet;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import jdk.internal.reflect.CallerSensitive;

@Test(groups = { "level.extended" })
@SuppressWarnings("nls")
public class StackWalkerCallerSensitiveTest {
	static final String TEST_MYLOADER_INSTANCE = "TestMyloaderInstance";
	protected static Logger logger = Logger.getLogger(StackWalkerCallerSensitiveTest.class);
	private String testName;

	@Test
	public void testGCCfromCallerSensitive() {
		StackWalker w = StackWalker.getInstance(new HashSet<Option>(Arrays.asList(new Option[] {Option.RETAIN_CLASS_REFERENCE, Option.SHOW_REFLECT_FRAMES, Option.SHOW_HIDDEN_FRAMES})));
		boolean threwUOE = false;
		try {
			getCCCallerSensitive(w);
		} catch (UnsupportedOperationException e) {
			threwUOE = true;
			logger.info("UnsupportedOperationException thrown");
		} catch (Exception e) {
			fail("Unexpected exception "+e, e);
		}
		assertTrue(threwUOE, "Expected UnsupportedOperationException not thrown");
	}

	@Test
	public void testGCCfromNonCallerSensitive() {
		StackWalker w = StackWalker.getInstance(new HashSet<Option>(Arrays.asList(new Option[] {Option.RETAIN_CLASS_REFERENCE, Option.SHOW_REFLECT_FRAMES, Option.SHOW_HIDDEN_FRAMES})));
		try {
			getCC(w);
		} catch (Exception e) {
			fail("Unexpected exception "+e, e);
		}
	}

	@Test
	public void testGCCfromNonCallerSensitiveCalledFromCallerSensitive() {
		StackWalker w = StackWalker.getInstance(new HashSet<Option>(Arrays.asList(new Option[] {Option.RETAIN_CLASS_REFERENCE, Option.SHOW_REFLECT_FRAMES, Option.SHOW_HIDDEN_FRAMES})));
		try {
			getCC(w);
		} catch (Exception e) {
			fail("Unexpected exception "+e, e);
		}
	}

	@Test
	public void testGCCfromCallerSensitiveCalledFromCallerSensitive() {
		StackWalker w = StackWalker.getInstance(new HashSet<Option>(Arrays.asList(new Option[] {Option.RETAIN_CLASS_REFERENCE, Option.SHOW_REFLECT_FRAMES, Option.SHOW_HIDDEN_FRAMES})));
		try {
			callerSensitiveCaller(w);
		} catch (Exception e) {
			fail("Unexpected exception "+e, e);
		}
	}

	@CallerSensitive
	private Class callerSensitiveCaller(StackWalker w) {
		logger.info("getCallerClass() from caller-sensitive wrapper method");
		return getCC(w);
	}

	private  Class  getCC(StackWalker w) {
		logger.info("getCC entered");
		return w.getCallerClass();
	}
	
	@CallerSensitive
	public  Class  getCCCallerSensitive(StackWalker w) {
		logger.info("getCCCallerSensitive entered");
		return w.getCallerClass();
	}

}
