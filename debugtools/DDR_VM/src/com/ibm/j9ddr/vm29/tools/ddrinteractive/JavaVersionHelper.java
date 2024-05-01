/*
 * Copyright IBM Corp. and others 2018
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;

/**
 * JavaVersionHelper helps check if the JVM version is new enough for the modularity DDR commands
 */
public class JavaVersionHelper 
{
	private final static int J2SE_SERVICE_RELEASE_MASK = 0xFFFF;
	private final static int J2SE_JAVA_SPEC_VERSION_SHIFT = 8;
	
	/**
	 * Returns {@code true} if the Java version is Java9 or higher.
	 * @param vm J9JavaVMPointer
	 * @param out the output print stream
	 * @return {@code true} if the Java version is Java9 or higher
	 * @throws CorruptDataException
	 * @deprecated use {@link #ensureMinimumJavaVersion(int, J9JavaVMPointer, PrintStream)} instead
	 */
	@Deprecated
	public static boolean ensureJava9AndUp(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException
	{
		return ensureMinimumJavaVersion(9, vm, out);
	}

	/**
	 * Returns {@code true} if the Java version is {@code version} or higher.
	 * @param version the minimum version
	 * @param vm J9JavaVMPointer
	 * @param out the output print stream
	 * @return {@code true} if the Java version is {@code version} or higher
	 * @throws CorruptDataException
	 */
	public static boolean ensureMinimumJavaVersion(int version, J9JavaVMPointer vm, PrintStream out) throws CorruptDataException
	{
		int javaVersion = vm.j2seVersion().bitAnd(J2SE_SERVICE_RELEASE_MASK).rightShift(J2SE_JAVA_SPEC_VERSION_SHIFT).intValue();
		if (javaVersion < version) {
			out.printf("This command only works with core file created by VM with Java version %d or higher.%n"
					+ "The current VM Java version is: %s.%n", version, javaVersion);
			return false;
		}
		return true;
	}
}
