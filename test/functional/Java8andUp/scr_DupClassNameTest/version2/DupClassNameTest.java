/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
public class DupClassNameTest {
	public static void getStackTrace() {
		System.out.println("running method getStackTrace of DupClassNameTest version2:");
		StackTraceElement[] st = Thread.currentThread().getStackTrace();
	}
	public static void foo() {
		System.out.println("running method foo of DupClassNameTest version2");
	}
	public static void foo1() {
		System.out.println("running method foo1 of DupClassNameTest version2");
	}
	public static void foo2() {
		System.out.println("running method foo2 of DupClassNameTest version2");
	}
	public static void foo3() {
		System.out.println("running method foo3 of DupClassNameTest version2");
	}
	public static void foo4() {
		System.out.println("running method foo4 of DupClassNameTest version2");
	}
	public static void foo5() {
		System.out.println("running method foo5 of DupClassNameTest version2");
	}
}
