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
package com.ibm.jvmti.tests.decompileAtMethodResolve;

public class ResolveTest2Super extends ResolveTest {

	public static long s_j1 = (long)-2;
	public static long s_j2 = (long)-3;
	public static long s_j3 = (long)4;
	public static long s_j4 = (long)-5;

	public void forceDecompile(/*Object this, */ long j1, long j2, long j3, long j4) {
		error("super forceDecompile called");
	}

	public void test() {
		startTest("Longs");
		forceDecompile(s_j1, s_j2, s_j3, s_j4);
		endTest("Longs");
	}

}
