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

package com.ibm.jvmti.tests.nestMatesRedefinition;

import org.objectweb.asm.*;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

public class ClassGenerator implements Opcodes {
	
	public static byte[] generateClass(String name, RedefinitionPhase phase, Attribute[] attributes) {
		ClassWriter classWriter = new ClassWriter(ClassWriter.COMPUTE_MAXS | ClassWriter.COMPUTE_FRAMES);
		classWriter.visit(V1_8, ACC_PUBLIC | ACC_SUPER, name, null, "java/lang/Object", null);
		
		switch(phase) {
		case ORIGINAL:
			/* Original has the source attribute */
			classWriter.visitSource(name + ".java", null);	
			break;
		case REDEFINE:
			/* REDEFINE has no source attribute to ensure the VM can't skip the redefine */
			break;
		default:
			throw new IllegalArgumentException("Phase must be set correctly");
		}
		
		/* Generate base constructor which just calls super.<init>() */
		MethodVisitor methodVisitor = classWriter.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		methodVisitor.visitCode();
		methodVisitor.visitVarInsn(ALOAD, 0);
		methodVisitor.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		methodVisitor.visitInsn(RETURN);
		methodVisitor.visitMaxs(1, 1);
		methodVisitor.visitEnd();

		/* Generate passed in attributes if there are any */
		if (null != attributes) {
			for (Attribute attr : attributes) {
				classWriter.visitAttribute(attr);
			}
		}

		classWriter.visitEnd();
		return classWriter.toByteArray();
	}

	static byte[] testClassNoAttribute () throws Exception {
		return generateClass("TestClass", RedefinitionPhase.ORIGINAL, null);
	}
	
	static byte[] testClassNoAttributeForceRedefinition () throws Exception {
		return generateClass("TestClass", RedefinitionPhase.REDEFINE, null);
	}

	static byte[] testClassWithNestMembersAttribute () throws Exception {
		String[] nestMembers = {"A", "B", "C"};
		NestMembersAttribute attr = new NestMembersAttribute(nestMembers);
		return generateClass("TestClass", RedefinitionPhase.ORIGINAL, new Attribute[]{ attr });
	}
	
	static byte[] testClassWithNestMembersAttributeForceRedefinition () throws Exception {
		String[] nestMembers = {"A", "B", "C"};
		NestMembersAttribute attr = new NestMembersAttribute(nestMembers);
		return generateClass("TestClass", RedefinitionPhase.REDEFINE, new Attribute[]{ attr });
	}

	static byte[] nestMemberNoAttributeForceRedefinition () throws Exception {
		return generateClass("TestClass", RedefinitionPhase.REDEFINE, null);
	}

	static byte[] testClassWithNestHostAttribute () throws Exception {
		NestHostAttribute attr = new NestHostAttribute("host");
		return generateClass("TestClass", RedefinitionPhase.ORIGINAL, new Attribute[]{ attr });
	}
	
	static byte[] testClassWithNestHostAttributeForceRedefinition () throws Exception {
		NestHostAttribute attr = new NestHostAttribute("host");
		return generateClass("TestClass", RedefinitionPhase.REDEFINE, new Attribute[]{ attr });
	}

	static byte[] testClassWithAlteredNestMembersAttributeData () throws Exception {
		String[] nestMembers = {"D", "E", "F"};
		NestMembersAttribute attr = new NestMembersAttribute(nestMembers);
		return generateClass("TestClass", RedefinitionPhase.ORIGINAL, new Attribute[]{ attr });
	}

	static byte[] testClassWithAlteredNestMembersAttributeLength () throws Exception {
		String[] nestMembers = {"A", "B"};
		NestMembersAttribute attr = new NestMembersAttribute(nestMembers);
		return generateClass("TestClass", RedefinitionPhase.ORIGINAL, new Attribute[]{ attr });
	}

	static byte[] testClassWithAlteredNestHostAttribute () throws Exception {
		NestHostAttribute attr = new NestHostAttribute("AlteredHostName");
		return generateClass("TestClass", RedefinitionPhase.ORIGINAL, new Attribute[]{ attr });
	}
}
