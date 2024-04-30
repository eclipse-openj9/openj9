/*
 * Copyright IBM Corp. and others 2023
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.util;

import org.testng.Assert;

/**
 * This provides test access to the API of java.lang.Compiler, which was
 * removed in Java 21. This class simply reuses the native implementations
 * from java.lang.Compiler.
 */
public final class CompilerAccess {

	static {
		System.loadLibrary("access");
		if (!registerNatives()) {
			Assert.fail("failed to register natives");
		}
	}

	public static Object command(Object cmd) {
		if (cmd == null) {
			throw new NullPointerException();
		}
		return commandImpl(cmd);
	}

	private static native Object commandImpl(Object cmd);

	public static boolean compileClass(Class<?> classToCompile) {
		if (classToCompile == null) {
			throw new NullPointerException();
		}
		return compileClassImpl(classToCompile);
	}

	private static native boolean compileClassImpl(Class classToCompile);

	public static boolean compileClasses(String nameRoot) {
		if (nameRoot == null) {
			throw new NullPointerException();
		}
		return compileClassesImpl(nameRoot);
	}

	private static native boolean compileClassesImpl(String nameRoot);

	public static native void disable();

	public static native void enable();

	private static native boolean registerNatives();

}
