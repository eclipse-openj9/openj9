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
package org.openj9.test.javaagenttest.util;

import java.io.IOException;
import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.IllegalClassFormatException;
import java.security.ProtectionDomain;

import jdk.internal.org.objectweb.asm.ClassReader;
import jdk.internal.org.objectweb.asm.ClassVisitor;
import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.Opcodes;
import jdk.internal.org.objectweb.asm.tree.FieldNode;
import jdk.internal.org.objectweb.asm.util.CheckClassAdapter;
import org.openj9.test.javaagenttest.AgentMain;

public class Transformer implements ClassFileTransformer {

	private int flag = 0;
	private String _methodName;
	public static final int MODIFY_EXISTING_METHOD = 1;
	public static final int ADD_NEW_METHOD = 2;
	public static final int ADD_NEW_FIELD = 3;

	public Transformer(int flag) {
		this.flag = flag;
	}

	public Transformer(int flag, String methodName) {
		this.flag = flag;
		_methodName = methodName;
	}

	public byte[] transform(ClassLoader loader, String className, Class<?> classToTransform,
			ProtectionDomain protectionDomain, byte[] classfileBuffer) throws IllegalClassFormatException {
		try {
			if (flag == MODIFY_EXISTING_METHOD) { // for fast HCR
				AgentMain.logger.debug("Instrumenting " + className + " by modifying existing method.");
				ClassReader reader = new ClassReader(classToTransform.getCanonicalName());
				ClassWriter cw = new ClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
				ClassVisitor visitor = new CustomClassVisitor(cw, classToTransform.getCanonicalName());
				reader.accept(visitor, ClassReader.EXPAND_FRAMES);
				return cw.toByteArray();
			} else if (flag == ADD_NEW_METHOD) { // for extended HCR
				AgentMain.logger.debug("Instrumenting " + className + " by adding a new method.");
				ClassReader reader = new ClassReader(classToTransform.getCanonicalName());
				ClassWriter cw = new ClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
				ClassVisitor cv = new MethodAddAdapter(new CheckClassAdapter(cw));
				reader.accept(cv, ClassReader.SKIP_FRAMES);
				return cw.toByteArray();
			} else if (flag == ADD_NEW_FIELD) { // for GCRetransform test
				AgentMain.logger.debug("Instrumenting " + className + " by adding a new field.");
				ClassReader reader = new ClassReader(classToTransform.getCanonicalName());
				ClassWriter cw = new ClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
				FieldNode newField = new FieldNode(Opcodes.ACC_PUBLIC | Opcodes.ACC_STATIC, "new_static_int_array",
						"[I", null, null);
				ClassVisitor cv = new FieldAdder(new CheckClassAdapter(cw), newField, className, _methodName);
				reader.accept(cv, ClassReader.SKIP_FRAMES);
				return cw.toByteArray();
			}
			return null;
		} catch (IOException e) {
			return null;
		}
	}
}
