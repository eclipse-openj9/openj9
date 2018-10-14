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

import java.lang.invoke.*;
public class BootstrapMethods {

	public static Object bootstrap_constant_string(MethodHandles.Lookup l, String name, Class<?> c, String s) {
		return s;
	}

	public static int bootstrap_constant_int(MethodHandles.Lookup l, String name, Class<?> c, int v) {
		return v;
	}

	public static float bootstrap_constant_float(MethodHandles.Lookup l, String name, Class<?> c, float v) {
		return v;
	}

	public static double bootstrap_constant_double(MethodHandles.Lookup l, String name, Class<?> c, double v) {
		return v;
	}

	public static long bootstrap_constant_long(MethodHandles.Lookup l, String name, Class<?> c, long v) {
		return v;
	}
	public static int bootstrap_constant_int_exception(MethodHandles.Lookup l, String name, Class<?> c, int v) throws Exception {
		throw new Exception();
	}
}
