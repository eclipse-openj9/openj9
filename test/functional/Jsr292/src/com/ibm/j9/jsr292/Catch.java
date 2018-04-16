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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;

public class Catch {

	static class FooException extends Exception {
		private static final long serialVersionUID = 1L; 
	};

	@Test(groups = { "level.extended" })
	public void testCatcher() throws Throwable {
        MethodHandle thrower = MethodHandles.foldArguments(
                MethodHandles.throwException(boolean.class, FooException.class),
                MethodHandles.lookup().findConstructor(FooException.class, methodType(void.class)));

                MethodHandle catcher = MethodHandles.dropArguments(MethodHandles.constant(boolean.class, true), 0, FooException.class);
                MethodHandle runCatcher = MethodHandles.catchException(thrower, FooException.class, catcher);
                boolean b = (boolean)runCatcher.invokeExact();
                AssertJUnit.assertTrue(b);
	}
}
