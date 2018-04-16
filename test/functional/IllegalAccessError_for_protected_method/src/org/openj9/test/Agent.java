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
package org.openj9.test;

import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.IllegalClassFormatException;
import java.lang.instrument.Instrumentation;
import java.security.ProtectionDomain;

import org.objectweb.asm.*;
import org.testng.log4testng.Logger;

/**
 * Agent will retransform "test.Testcase" by adding code to the
 * beginning of each method.  The important thing is that it will
 * result in two definitions of the test.Testcase class:
 * 	1) original class
 * 	2) the new class
 *
 */
public class Agent implements ClassFileTransformer, Opcodes {
	public static Logger logger = Logger.getLogger(Agent.class);

	public static Instrumentation instrumentation;
	public static Agent agent = new Agent();

	public static void premain(String agentArgs, Instrumentation inst) {
		logger.debug("Agent premain called");
		inst.addTransformer(agent, true);
		instrumentation = inst;
	}

	static int count = 0;

	@Override
	public byte[] transform(ClassLoader loader, String name, Class<?> clazz, ProtectionDomain pd, byte[] bytes)
			throws IllegalClassFormatException {
		if (name.equals("test/Testcase")) {
			if (count >= 1) {
				logger.debug("About to redefine (" + name + ")");
				ClassReader reader = new ClassReader(bytes);
				ClassWriter cw = new ClassWriter(reader, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
				ClassVisitor visitor = new AgentClassVisitor(ASM4, cw);
				reader.accept(visitor, ClassReader.SKIP_FRAMES); // SKIP_FRAMES is a perf win when using CW.COMPUTE_FRAMES
				byte[] newBytes = cw.toByteArray();
				logger.debug("Returning new bytes");
				return newBytes;
			}
			count++;
		}
		return null;
	}

}

class AgentClassVisitor extends ClassVisitor {

	public AgentClassVisitor(int asmVersion, ClassVisitor cv) {
		super(asmVersion, cv);
	}

	@Override
	public MethodVisitor visitMethod(int access, String name, String desc, String signature, String[] exceptions) {
		MethodVisitor mv = super.visitMethod(access, name, desc, signature, exceptions);
		/* ignore <init> & <clinit> */
		if (name.indexOf('<') < 0) {
			mv = new AgentMethodVisitor(Opcodes.ASM4, mv);
		}
		return mv;
	}
}

class AgentMethodVisitor extends MethodVisitor implements Opcodes {

	public AgentMethodVisitor(int asmVersion, MethodVisitor mv) {
		super(asmVersion, mv);
	}

	@Override
	public void visitCode() {
		mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
		mv.visitLdcInsn("**IN INSTRUMENTED METHOD**");
		mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V");
		super.visitCode();
	}

}
