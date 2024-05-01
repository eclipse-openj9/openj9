package org.openj9.test.utilities;

/*
 * Copyright IBM Corp. and others 2020
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

 import org.objectweb.asm.*;
 import org.openj9.test.util.VersionCheck;

 public class SealedClassGenerator implements Opcodes {
	private static String dummySubclassName = "TestSubclassGen";
	/* sealed classes are NOT a preview feature since LTS jdk 17 */
	private static int latestVersion = VersionCheck.classFile();

	public static byte[] generateFinalSealedClass(String className) {
		int accessFlags = ACC_FINAL | ACC_SUPER;

		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(latestVersion, accessFlags, className, null, "java/lang/Object", null);

		/* Sealed class must have a PermittedSubclasses attribute */
		cw.visitPermittedSubclass(dummySubclassName);

		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] generateSubclassIllegallyExtendingSealedSuperclass(String className, Class<?> superClass) {
		String superName = superClass.getName().replace('.', '/');
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(latestVersion, ACC_SUPER, className, null, superName, null);
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] generateSubinterfaceIllegallyExtendingSealedSuperinterface(String className, Class<?> superInterface) {
		String[] interfaces = { superInterface.getName().replace('.', '/') };
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(latestVersion, ACC_SUPER, className, null, "java/lang/Object", interfaces);
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] generateSealedClass(String className, String[] permittedSubclassNames) {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(latestVersion, ACC_SUPER, className, null, "java/lang/Object", null);

		for (String psName : permittedSubclassNames) {
			cw.visitPermittedSubclass(psName);
		}

		cw.visitEnd();
		return cw.toByteArray();
	}
 }
