/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse License, v. 2.0 are satisfied: GNU
 * General License, version 2 with the GNU Classpath
 * Exception [1] and GNU General License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

package org.openj9.test.java_lang_invoke.helpers;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.VarHandle;

public class Jep334MHHelperImpl implements Jep334MHHelper {
	/*
	 * MethodHandle helper fields & methods
	 */
	/* fields */
	public volatile int getterTest = 1;
	public int setterTest = 2;
	public static volatile int staticGetterTest = 3;
	public static int staticSetterTest = 4;

	/* constructors */
	public Jep334MHHelperImpl() {}

	public Jep334MHHelperImpl(String s, int i) {}

	/* methods to invoke */
	public void virtualTest() {}

	public void specialTest() {}

	public static void staticTest() {}

	/* special helper */
	public static MethodHandles.Lookup lookup() {
		return MethodHandles.lookup();
	}

	/*
	 * VarHandle helper fields & methods
	 */

	/* test types */
	final public static int array_test = 1;
	final public static int instance_test = 2;
	final public static int static_test = 3;

	public static int staticTest;
	public Integer instanceTest;

	public static VarHandle getInstanceTest() throws Throwable {
		return MethodHandles.lookup().findVarHandle(Jep334MHHelperImpl.class, "instanceTest", Integer.class);
	}

	public static VarHandle getStaticTest() throws Throwable {
		return MethodHandles.lookup().findStaticVarHandle(Jep334MHHelperImpl.class, "staticTest", int.class);
	}

	public static VarHandle getArrayTest() throws Throwable {
		return MethodHandles.arrayElementVarHandle(double[].class);
	}

	public static VarHandle getArrayTest2() throws Throwable {
		return MethodHandles.arrayElementVarHandle(Float[][].class);
	}
}

