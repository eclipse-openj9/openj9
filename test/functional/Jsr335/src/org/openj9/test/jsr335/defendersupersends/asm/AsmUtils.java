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
package org.openj9.test.jsr335.defendersupersends.asm;

import java.io.IOException;

public class AsmUtils {
	private static AsmLoader loader = new AsmLoader(AsmUtils.class.getClassLoader());
	
	/**
	 * Entry point for the test, returns the junit test to run
	 * @return the junit test to run
	 */
	public static Class<?> getDefenderSupersendTestcase(){
		AsmTestcaseGenerator gen = new AsmTestcaseGenerator(loader, new String[]{"m", "w"});
		return gen.getDefenderSupersendTestcase();
	}
	
	/**
	 * Makes it easy to allocate new objects by attaching the package and calling to the shim-loader
	 * @param clazz Short-name of the class to allocate ("A" turns into "java/lang/A", depending on the value of AsmGenerator.pckg)
	 * @return An allocated object
	 * @throws ClassNotFoundException
	 * @throws IllegalAccessException
	 * @throws InstantiationException
	 */
	public static Object newInstance(String clazz) throws ClassNotFoundException, IllegalAccessException, InstantiationException{
		String fullName = AsmTestcaseGenerator.pckg + "/" +clazz;
		fullName = fullName.replace('/', '.');
		Class<?> c = loader.loadClass(fullName);
		Object o = c.newInstance();
		return o;
	}

	public static void dumpClasses(String destination) throws IOException {
		/* This will setup and load the testcases into the loader */
		getDefenderSupersendTestcase();
		loader.dumpAsmClasses(destination);
	}
}
