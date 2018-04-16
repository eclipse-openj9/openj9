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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import static java.lang.invoke.MethodType.methodType;
import java.lang.invoke.MethodHandles;

public class Crash {
	
	//crashes in Oracle
	@Test(groups = { "level.extended" })
	public static void testIllegalArgument() throws Throwable{
		boolean exceptionOccurred = false;
		try {
			long l = (long)MethodHandles.publicLookup()
				 	.findStatic(Crash.class, "add", methodType(long.class, long.class, long.class))
				 	.asSpreader(long[].class, 1)
				 	.invoke(1l, (long[]) null);
		} catch(IllegalArgumentException e) {
			exceptionOccurred = true;
		}
		AssertJUnit.assertTrue(exceptionOccurred);
	}
	
	public static long add(long a, long b) { return a + b; }
}
