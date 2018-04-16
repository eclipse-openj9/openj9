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
package com.ibm.j9.jsr292.indyn;


import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_STATIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.ARETURN;
import static org.objectweb.asm.Opcodes.IRETURN;
import static org.objectweb.asm.Opcodes.H_INVOKESTATIC;
import static org.objectweb.asm.Opcodes.V1_7;
import static org.objectweb.asm.Opcodes.*;
import java.io.FileOutputStream;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Type;

public class ComplexIndyGenerator {

	public static void main(String[] args) throws Throwable {
		FileOutputStream fos = new FileOutputStream("ComplexIndy.class");
		fos.write(makeExample());
		fos.flush();
		fos.close();
	}

	public static byte[] makeExample() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, "com/ibm/j9/jsr292/indyn/ComplexIndy", null, "java/lang/Object", null);

		MethodVisitor mv;
		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "gwtTest", "(Ljava/lang/Object;)Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitInvokeDynamicInsn("gwtBootstrap", "(Ljava/lang/Object;)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"gwtBootstrap",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor())
					);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "permuteTest", "(IILjava/lang/Object;)Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitVarInsn(ILOAD, 0);
			mv.visitVarInsn(ILOAD, 1);
			mv.visitVarInsn(ALOAD, 2);
			mv.visitInvokeDynamicInsn("gwtBootstrap", "(IILjava/lang/Object;)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"permuteBootstrap",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor())
					);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "switchpointTest", "(II)I", null, null);
			mv.visitCode();
			mv.visitVarInsn(ILOAD, 0);
			mv.visitVarInsn(ILOAD, 1);
			mv.visitInvokeDynamicInsn("switchpointBootstrap", "(II)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"switchpointBootstrap",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor())
					);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "mcsTest", "(II)I", null, null);
			mv.visitCode();
			mv.visitVarInsn(ILOAD, 0);
			mv.visitVarInsn(ILOAD, 1);
			mv.visitInvokeDynamicInsn("mcsBootstrap", "(II)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"mcsBootstrap",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor())
					);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "catchTest", "(II)I", null, null);
			mv.visitCode();
			mv.visitVarInsn(ILOAD, 0);
			mv.visitVarInsn(ILOAD, 1);
			mv.visitInvokeDynamicInsn("catchBootstrap", "(II)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"catchBootstrap",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor())
					);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "foldTest", "(I)I", null, null);
			mv.visitCode();
			mv.visitVarInsn(ILOAD, 0);
			mv.visitInvokeDynamicInsn("foldBootstrap", "(I)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"foldBootstrap",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor())
					);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}

		{
			Label L1 = new Label();
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "fibIndy", "(I)I", null, null);
			mv.visitCode();
			mv.visitVarInsn(ILOAD, 0);
			
			mv.visitInsn(ICONST_1);
			mv.visitJumpInsn(IF_ICMPGT, L1);
			mv.visitInsn(ICONST_1);
			mv.visitInsn(IRETURN);
			
			mv.visitLabel(L1);
			mv.visitVarInsn(ILOAD, 0);
			mv.visitInsn(ICONST_1);
			mv.visitInsn(ISUB);
			
			mv.visitInvokeDynamicInsn("fibBootstrap", "(I)I",
				new Handle(
					H_INVOKESTATIC, 
					"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
					"fibBootstrap",
					Type.getType(
						"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
					).getDescriptor())
				);
			
			mv.visitVarInsn(ILOAD, 0);
			mv.visitInsn(ICONST_2);
			mv.visitInsn(ISUB);
			
			mv.visitInvokeDynamicInsn("fibBootstrap", "(I)I",
					new Handle(
						H_INVOKESTATIC, 
						"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
						"fibBootstrap",
						Type.getType(
							"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
						).getDescriptor())
					);
			
			mv.visitInsn(IADD);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}
		
		cw.visitEnd();
		return cw.toByteArray();
	}

}
