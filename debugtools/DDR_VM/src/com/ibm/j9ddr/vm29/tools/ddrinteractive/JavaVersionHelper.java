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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * JavaVersionHelper helps check if the JVM version is new enough for the modularity DDR commands
 */
public class JavaVersionHelper 
{
	public final static int J2SE_SERVICE_RELEASE_MASK = 0xFFFF;
	public final static int J2SE_19 = 9;
	public final static int J2SE_JAVA_SPEC_VERSION_SHIFT = 8;
	
	/**
	 * Returns true if the Java version is Java9 and up.
	 * @param vm J9JavaVMPointer
	 * @param out The output print stream
	 * @return if the Java version is Java9 and up or not.
	 * @throws CorruptDataException
	 */
	public static boolean ensureJava9AndUp(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException 
	{
		int javaVersion = vm.j2seVersion().bitAnd(J2SE_SERVICE_RELEASE_MASK).intValue() >> J2SE_JAVA_SPEC_VERSION_SHIFT;
		if (javaVersion < J2SE_19) {
			out.printf("This command only works with core file created by VM with Java version 9 or higher%n"
					+ "The current VM Java version is: %s%n", javaVersion);
			return false;
		}
		return true;
	}
}
