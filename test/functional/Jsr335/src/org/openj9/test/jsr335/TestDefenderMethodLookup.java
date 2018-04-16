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
package org.openj9.test.jsr335;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.*;
import java.lang.invoke.MethodHandles.Lookup;

@Test(groups = { "level.sanity" })
public class TestDefenderMethodLookup {
	/**
	 * Get a <b>SPECIAL</b> MethodHandle for the method "test()V" in the <b>DIRECT</b> super interface DefenderInterface. The method
	 * has a default implementation in DefenderInterface and does <b>NOT</b> have an implementation in the class.
	 * Invoke the MethodHandle, and assert that the DefenderInterface.test was invoked (should return "default").
	 *
	 * @throws Throwable No exceptions is expected. Any exception should be treated as an error.
	 */
	@Test
	public void testDirectSuperInterface() throws Throwable {
		DefenderInterface impl = new DefenderInterface() {
			public MethodHandle run() throws Throwable {
				Lookup l = DefenderInterface.lookup();
				Class<? extends DefenderInterface> defc = this.getClass();
				Class<DefenderInterface> target = DefenderInterface.class;
				MethodType mt = MethodType.methodType(String.class);
				return l.findSpecial(defc, "test", mt, target);
			}
		};
		MethodHandle mh = impl.run();
		String result = (String)mh.invoke(impl);
		AssertJUnit.assertEquals("default", result);
	}

	/**
	 * Same as <b>testDirectSuperInterface</b>, but with the findSpecial arguments <b>target</b> and <b>defc</b> switched.
	 *
	 * @throws Throwable No exceptions is expected. Any exception should be treated as an error.
	 */
	@Test
	public void testDirectSuperInterfaceSwitchedTargetDefc() throws Throwable {
		DefenderInterface impl = new DefenderInterface() {
			public MethodHandle run() throws Throwable {
				Lookup l = MethodHandles.lookup();
				Class<? extends DefenderInterface> defc = this.getClass();
				Class<DefenderInterface> target = DefenderInterface.class;
				MethodType mt = MethodType.methodType(String.class);
				// Switched target and defc
				return l.findSpecial(target, "test", mt, defc);
			}
		};
		MethodHandle mh = impl.run();
		String result = (String)mh.invoke(impl);
		AssertJUnit.assertEquals("default", result);
	}

	/**
	 * Get a <b>SPECIAL</b> MethodHandle for the method "test()V" in the <b>DIRECT</b> super interface DefenderInterface. The method
	 * has a default implementation in DefenderInterface and does <b>ALSO</b> have an implementation in the class.
	 * Invoke the MethodHandle, and assert that the DefenderInterface.test was invoked (should return "default").
	 *
	 * @throws Throwable No exceptions is expected. Any exception should be treated as an error.
	 */
	@Test
	public void testDirectSuperInterfaceWithOverride() throws Throwable {
		DefenderInterface impl = new DefenderInterface() {
			@Test
			@Override
			public String test() {
				return "impl";
			}

			public MethodHandle run() throws Throwable {
				Lookup l = DefenderInterface.lookup();
				Class<? extends DefenderInterface> defc = DefenderInterface.class;
				Class<DefenderInterface> target = DefenderInterface.class;
				MethodType mt = MethodType.methodType(String.class);
				return l.findSpecial(defc, "test", mt, target);
			}
		};
		MethodHandle mh = impl.run();
		String result = (String)mh.invoke(impl);
		AssertJUnit.assertEquals("default", result);
	}

	/**
	 * Same as <b>testDirectSuperInterfaceWithOverride</b>, but with the findSpecial arguments <b>target</b> and <b>defc</b> switched.
	 *
	 * @throws Throwable No exceptions is expected. Any exception should be treated as an error.
	 */
	@Test
	public void testDirectSuperInterfaceWithOverrideSwitchedTargetDefc() throws Throwable {
		DefenderInterface impl = new DefenderInterface() {
			@Override
			public String test() {
				return "impl";
			}

			public MethodHandle run() throws Throwable {
				Lookup l = MethodHandles.lookup();
				Class<? extends DefenderInterface> defc = this.getClass();
				Class<DefenderInterface> target = DefenderInterface.class;
				MethodType mt = MethodType.methodType(String.class);
				// Switched target and defc
				return l.findSpecial(target, "test", mt, defc);
			}
		};
		MethodHandle mh = impl.run();
		String result = (String)mh.invoke(impl);
		AssertJUnit.assertEquals("default", result);
	}

	/**
	 * <b>NEGATIVE</b><br />
	 * Try to get a <b>SPECIAL</b> MethodHandle for the method "test()V" in the <b>INDIRECT</b> super interface DefenderInterface
	 * (through the interface <b>DefenderSubInterface</b>).
	 *
	 * @throws Throwable Expected exceptions are caught. Any other exception should be treated as an error.
	 */
	@Test
	public void testIndirectSuperInterface() throws Throwable {
		DefenderSubInterface impl = new DefenderSubInterface() {
			public MethodHandle run() throws Throwable {
				Lookup l = DefenderSubInterface.lookup();
				Class<? extends DefenderInterface> defc = this.getClass();
				Class<DefenderInterface> target = DefenderInterface.class;
				MethodType mt = MethodType.methodType(String.class);
				return l.findSpecial(defc, "test", mt, target);
			}
		};
		try {
			impl.run();
			Assert.fail("Successfully created supersend MethodHandle to INDIRECT super interface. Should fail with IllegalAccessException.");
		} catch (IllegalAccessException e) {}
	}
}
