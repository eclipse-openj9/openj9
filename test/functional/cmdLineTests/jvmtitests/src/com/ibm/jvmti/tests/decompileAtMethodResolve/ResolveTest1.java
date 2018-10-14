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

public class ResolveTest1 extends ResolveTest1Super {

	public void forceDecompile(/*Object this, */ int i1, long j1, double d1, float f1, double d2, float f2, double d3) {
		assertEquals("i1", i1, s_i1);
		assertEquals("j1", j1, s_j1);
		assertEquals("d1", d1, s_d1);
		assertEquals("f1", f1, s_f1);
		assertEquals("d2", d2, s_d2);
		assertEquals("f2", f2, s_f2);
		assertEquals("d3", d3, s_d3);
	}

}
