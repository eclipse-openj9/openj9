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
package org.openj9.test.condy;

import static org.objectweb.asm.Opcodes.ACC_PRIVATE;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_STATIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.ARETURN;
import static org.objectweb.asm.Opcodes.IRETURN;
import static org.objectweb.asm.Opcodes.H_INVOKESTATIC;
import static org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static org.objectweb.asm.Opcodes.RETURN;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.*;

import java.util.*;
import java.io.*;

/**
 *
 * Helper Class to create condyClass
 *
 */
public class DefineCondyClass {
	public static byte[] getNullCondyClassBytes(String name) {
		ClassWriter cw = new ClassWriter(0);
		cw.visit( 55, ACC_PUBLIC | ACC_SUPER, name, null, "java/lang/Object", null );
		
		MethodVisitor mv;
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "getCondy", "()Ljava/lang/Object;", null, null );
			mv.visitCode();
			mv.visitLdcInsn( 
				new ConstantDynamic( "constant_NULL", "Ljava/lang/Object;",
					new Handle(
							H_INVOKESTATIC, 
							"java/lang/invoke/ConstantBootstraps", 
							"nullConstant",
							Type.getType( "(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/Class;)Ljava/lang/Object;").getDescriptor() 
							)
					)
				);
			mv.visitInsn( ARETURN );
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] getPrimitiveCondyClassBytes(String name) {
		ClassWriter cw = new ClassWriter(0);
		cw.visit( 55, ACC_PUBLIC | ACC_SUPER, name, null, "java/lang/Object", null );
		
		MethodVisitor mv;
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "getCondy", "()I", null, null );
			mv.visitCode();
			mv.visitLdcInsn( 
				new ConstantDynamic( "constant_int", "I",
					new Handle(
							H_INVOKESTATIC, 
							"org/openj9/test/condy/BootstrapMethods", 
							"bootstrap_constant_int",
							Type.getType(
								"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/Class;I)I"
								).getDescriptor()
							),
							123432
					)
				);
			mv.visitInsn( IRETURN );
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] getExceptionCondyClassBytes(String name) {
		ClassWriter cw = new ClassWriter(0);
		cw.visit( 55, ACC_PUBLIC | ACC_SUPER, name, null, "java/lang/Object", null );
		
		MethodVisitor mv;
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "getCondy", "()I", null, null );
			mv.visitCode();
			mv.visitLdcInsn( 
				new ConstantDynamic( "constant_int", "I",
					new Handle(
							H_INVOKESTATIC, 
							"org/openj9/test/condy/BootstrapMethods", 
							"bootstrap_constant_int_exception",
							Type.getType(
								"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/Class;I)I"
								).getDescriptor()
							),
							123432
					)
				);
			mv.visitInsn( IRETURN );
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
}
