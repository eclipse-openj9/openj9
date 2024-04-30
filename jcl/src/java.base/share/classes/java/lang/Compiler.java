/*[INCLUDE-IF JAVA_SPEC_VERSION < 21]*/
/*
 * Copyright IBM Corp. and others 1998
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
package java.lang;

/**
 * This class is a placeholder for environments which
 * explicitly manage the action of a "Just In Time"
 * compiler.
 *
 * @author		OTI
 * @version		initial
 *
 * @see			Cloneable
 */
/*[IF JAVA_SPEC_VERSION >= 9]*/
@Deprecated(forRemoval=true, since="9")
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
public final class Compiler {

private Compiler() {}

/**
 * Low level interface to the JIT compiler. Can return
 * any object, or null if no JIT compiler is available.
 *
 * @author		OTI
 * @version		initial
 *
 * @return		Object
 *					result of executing command
 * @param		cmd Object
 *					a command for the JIT compiler
 */
public static Object command(Object cmd) {
	if (cmd == null) {
		throw new NullPointerException();
	}
	return commandImpl(cmd);
}

private static native Object commandImpl(Object cmd);

/**
 * Compiles the class using the JIT compiler. Answers
 * true if the compilation was successful, or false if
 * it failed or there was no JIT compiler available.
 *
 * @return		boolean
 *					indicating compilation success
 * @param		classToCompile java.lang.Class
 *					the class to JIT compile
 */
public static boolean compileClass(Class<?> classToCompile) {
	if (classToCompile == null) {
		throw new NullPointerException();
	}
	return compileClassImpl(classToCompile);
}

private static native boolean compileClassImpl(Class classToCompile);

/**
 * Compiles all classes whose name matches the argument
 * using the JIT compiler. Answers true if the compilation
 * was successful, or false if it failed or there was no
 * JIT compiler available.
 *
 * @return		boolean
 *					indicating compilation success
 * @param		nameRoot String
 *					the string to match against class names
 */
public static boolean compileClasses(String nameRoot) {
	if (nameRoot == null) {
		throw new NullPointerException();
	}
	return compileClassesImpl(nameRoot);
}

private static native boolean compileClassesImpl(String nameRoot);

/**
 * Disable the JIT compiler
 */
public static native void disable();

/**
 * Enable the JIT compiler
 */
public static native void enable();
}
