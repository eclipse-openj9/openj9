/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.invoker.util;

import java.util.Hashtable;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;

public class HelperClassGenerator extends ClassGenerator {

	public HelperClassGenerator(String directory, String pkgName) {
		super(directory, pkgName);
	}

	public HelperClassGenerator(String directory, String pkgName, Hashtable<String, Object> characteristics) {
		super(directory, pkgName, characteristics);
	}

	@Override
	public byte[] getClassData() throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;

		className = packageName + "/" + HELPER_CLASS_NAME;

		cw.visit(V1_8, ACC_PUBLIC, className, null, "java/lang/Object", null);
		{
			mv = cw.visitMethod(0, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			String version = (String)characteristics.get(VERSION);
			MethodType methodType = (MethodType)characteristics.get(HELPER_METHOD_TYPE);
			if (methodType == MethodType.STATIC) {
				mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, HELPER_METHOD_NAME, HELPER_METHOD_SIG, null, null);
			} else if (methodType == MethodType.VIRTUAL) {
				mv = cw.visitMethod(ACC_PUBLIC, HELPER_METHOD_NAME, HELPER_METHOD_SIG, null, null);
			}
			mv.visitCode();
			if (methodType == MethodType.STATIC) {
				if (version == null) {
					mv.visitLdcInsn("Static " + HELPER_CLASS_NAME + "." + HELPER_METHOD_NAME);
				} else {
					mv.visitLdcInsn("Static " + HELPER_CLASS_NAME + "_" + version + "." + HELPER_METHOD_NAME);
				}
			} else {
				if (version == null) {
					mv.visitLdcInsn("Virtual " + HELPER_CLASS_NAME + "." + HELPER_METHOD_NAME);
				} else {
					mv.visitLdcInsn("Virtual " + HELPER_CLASS_NAME + "_" + version + "." + HELPER_METHOD_NAME);
				}
			}
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();
		data = cw.toByteArray();

		writeToFile();

		return data;
	}

}
