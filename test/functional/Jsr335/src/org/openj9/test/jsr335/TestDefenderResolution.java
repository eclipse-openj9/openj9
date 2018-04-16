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
package org.openj9.test.jsr335;

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import java.util.*;

import org.objectweb.asm.*;
import org.openj9.test.jsr335.interfaces.*;

public class TestDefenderResolution implements Opcodes{
	private static Logger logger = Logger.getLogger(TestDefenderResolution.class);
	
	/* Test that the order interfaces are implemented doesn't 
	 * change the resolution, in particular when dealing with 
	 * equivalentSets 
	 */
	
	/* class C_JK implements J, K {}	--> K.m 
	 * class C_KJ implements K, J {}	--> K.m
	 * class C_IJK
	 * class C_JIK
	 * class C_JKI
	 * class C_IKJ
	 * class C_KIJ
	 * class C_KJI
	 */
	
	@Test(groups = { "level.sanity" })
	public void test_interfaceImplementationOrder() throws InstantiationException, IllegalAccessException {
		GeneratedLoader gl = new GeneratedLoader(TestDefenderResolution.class);
		String[][] testcases = new String[][] {
				new String[]{ "J", "K" },
				new String[]{ "K", "J" },
				new String[]{ "I", "K", "J" },
				new String[]{ "I", "J", "K" },
				new String[]{ "K", "I", "J" },
				new String[]{ "J", "I", "K" },
				new String[]{ "K", "J", "I" },
				new String[]{ "J", "K", "I" }
		};
		Arrays.stream(testcases).forEach(interfaces -> {
			String[] className = new String[] { "C_" };
			ArrayList<String> implementedInterfaces = new ArrayList<>();
			
			Arrays.stream(interfaces).forEachOrdered(n -> {
				className[0] += n;
				implementedInterfaces.add(p(n));
			});
			
			String[] fullyQualifiedIntefaces = implementedInterfaces.toArray(new String[implementedInterfaces.size()]);
			logger.debug("TestCase: " + className[0] + " implements " + Arrays.toString(fullyQualifiedIntefaces));
			Class<?> C = gl.load(className[0], generateClass(className[0], fullyQualifiedIntefaces));
			try {
				J instance = (J)C.newInstance();
				AssertJUnit.assertEquals(new J(){}.m(), instance.m());
			} catch (Exception e) {
				e.printStackTrace();
				Assert.fail("");
			}
		});
	}

	/**
	 * Generate an empty class that implements the requested interfaces
	 * @param name The name of the generated class
	 * @param interfaces The set of interfaces to implement
	 * @return the byte[] for the class
	 */
	static byte[] generateClass(String name, String[] interfaces) {
		ClassWriter cw = new ClassWriter(0);
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, name, null, "java/lang/Object", interfaces);
		{
			MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	static String p(String s) {
		return "org/openj9/test/jsr335/interfaces/"+s;
	}
	
}

class GeneratedLoader extends ClassLoader {
	public GeneratedLoader(Class<?> owner) {
		super(owner.getClassLoader());
	}
	public Class<?> load(String name, byte[] bytes) {
		return defineClass(name, bytes, 0, bytes.length);
	}
	@Override
	protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
		return super.loadClass(name, resolve);
	}
}
