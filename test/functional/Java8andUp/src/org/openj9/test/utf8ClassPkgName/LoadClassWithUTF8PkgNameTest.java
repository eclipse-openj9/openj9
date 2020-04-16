/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

package org.openj9.test.utf8ClassPkgName;

import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.File;
import java.net.URL;
import java.net.URLClassLoader;

import org.objectweb.asm.*;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static org.objectweb.asm.Opcodes.RETURN;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeTest;

import org.openj9.test.util.VersionCheck;


/**
 * Verify whether VM successfully loads a class with a Chinese UTF8 package name from a jar file
 * Note: The specified jar file to to be loaded in this test is automatically generated so as to
 * avoid any issue with platform dependencies.
 * 
 */
@Test(groups = { "level.sanity" })
public class LoadClassWithUTF8PkgNameTest {
	
	private static byte[] classBytes;
	final static int classVersionJava8 = 52;
	final static String loadedPackagePath = "org/openj9/resources/utf8pkgname/loaded_Utf8Class_\u6b63\u5728\u52a0\u8f7d\u7c7b\u5305\u540d\u957f\u5ea6\u6570\u68c0\u67e5/";
	final static String loadedClassName = "Utf8ClassPackageNameTest";
	final static String loadedClassFileWithPackagePath = loadedPackagePath + loadedClassName + ".class";
	final static String loadedClassNameWithDotPath = loadedPackagePath.replace('/', '.') + loadedClassName;
	final static String jarFileNameWithPath = System.getProperty("user.dir") + "/" + "LoadClassWithUTF8PkgNameTest.jar";
	
	public static byte[] generateClassBytes(String loadedPackagePath, String loadedClassName, int classVersion) throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(classVersion, ACC_PUBLIC + ACC_SUPER, loadedPackagePath + loadedClassName, null, "java/lang/Object", null);
		cw.visitSource(loadedClassName + ".java", null);

		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			Label l0 = new Label();
			mv.visitLabel(l0);
			mv.visitLineNumber(29, l0);
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
			mv.visitInsn(RETURN);
			Label l1 = new Label();
			mv.visitLabel(l1);
			mv.visitLocalVariable("this", "L" + loadedPackagePath + loadedClassName + ";", null, l0, l1, 0);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
	
	@BeforeTest
	public void beforeTest() throws Exception {
		/* The generated class file is only used by the java version >= Java 8 */
		int classVersion = (VersionCheck.major() - 8) + classVersionJava8;
		classBytes = generateClassBytes(loadedPackagePath, loadedClassName, classVersion);
		
		/* write the class bytes into the specified jar file */
		OutputStream fos = new FileOutputStream(new File(jarFileNameWithPath));
		JarOutputStream jos = new JarOutputStream(fos);
		JarEntry jarEntry = new JarEntry(loadedClassFileWithPackagePath);
		jos.putNextEntry(jarEntry);
		jos.write(classBytes, 0, classBytes.length);
		jos.closeEntry();
		jos.close();
	}
	
	/**
	 * Test whether it successfully loads a class with a Chinese UTF8 package name from a jar file
	 * (loaded_Utf8Class_LoadingClassPackageNameLengthCheck in English) after identifying the length of
	 * the qualified UTF8 class name.
	 * 
	 * @throws Exception
	 */
	@Test
	static public void testLoadClassWithUTF8PackageName() throws Exception {
		File jarFile = new File(jarFileNameWithPath);
		URLClassLoader urlCldr = new URLClassLoader(new URL[] {jarFile.toURI().toURL()});
		Class<?> clazz = urlCldr.loadClass(loadedClassNameWithDotPath);
	}
}
