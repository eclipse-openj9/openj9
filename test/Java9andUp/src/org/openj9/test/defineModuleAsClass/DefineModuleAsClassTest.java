/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https:www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https:www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https:www.gnu.org/software/classpath/license.html
 * [2] http:openjdk.java.net/legal/assembly-exception.html
 *******************************************************************************/
package org.openj9.test.defineModuleAsClass;

import org.testng.annotations.*;
import org.testng.*;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;

/**
 * Validate that Java 9 modules will be rejected and the correct error thrown
 * if they are loaded by the J9 class loader. 
 * 
 * @author Theresa Mammarella
 */
@Test(groups = { "level.extended" })
public class DefineModuleAsClassTest {
	/**
	 * Creates bytecode stream for a module
	 * @param name name of class
	 * @return byte array of class
	 */
	public static byte[] generateModule(String name) {
		ClassWriter cw = new ClassWriter(0);

		cw.visit(53, Opcodes.ACC_MODULE, name, null, null, null);
		cw.visitEnd();

		return cw.toByteArray();
	}

	@Test
	public void testLoadModuleInClassLoader() {
		String name = "module-info";
		final byte[] classBytes = generateModule(name);

		MyClassLoader loader = new MyClassLoader();		
		Assert.assertTrue(loader.loadClass(name, classBytes));
	}

}

class MyClassLoader extends ClassLoader {
	/**
	 * Attempts to load class into JVM.
	 * @param name
	 * @param classBytes
	 * @return true if class loader rejects module with correct error, false otherwise
	 */
	public boolean loadClass(String name, byte[] classBytes) {
		boolean result = false;

		try {
			Class<?> testClass = defineClass(name, classBytes, 0, classBytes.length);
		} catch (ClassFormatError e) {
			String message = e.getMessage();
			if (message.contains("is not a class because access_flag ACC_MODULE is set")) {
				result = true;
			}
		}
		return result;
	}
}
