/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
package org.openj9.test.varhandle;

public class StaticHelper {
	static final int finalI = 1;
	
	static byte b;
	static char c;
	static double d;
	static float f;
	static int i;
	static long j;
	static String l1;
	static Class<?> l2;
	static short s;
	static boolean z;
	
	static void reset() {
		b = 1;
		c = '1';
		d = 1.0;
		f = 1.0f;
		i = 1;
		j = 1L;
		l1 = "1";
		l2 = String.class;
		s = 1;
		z = true;
	}
	
	static final class StaticNoInitializationHelper {
		static final int finalI = 1;
	}
}
