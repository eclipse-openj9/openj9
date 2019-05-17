/*[INCLUDE-IF Sidecar16]*/
package java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=true, since="9")
/*[ENDIF]*/
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
