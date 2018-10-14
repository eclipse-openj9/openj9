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

import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_STATIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.ARETURN;
import static org.objectweb.asm.Opcodes.H_INVOKESTATIC;
import static org.objectweb.asm.Opcodes.H_INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.ILOAD;
import static org.objectweb.asm.Opcodes.IRETURN;
import static org.objectweb.asm.Opcodes.RETURN;
import static org.objectweb.asm.Opcodes.LRETURN;
import static org.objectweb.asm.Opcodes.FRETURN;
import static org.objectweb.asm.Opcodes.DRETURN;


import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Type;
import org.objectweb.asm.*;

import java.util.*;
import java.io.*;

public class CondyGenerator extends ClassLoader{
   
	public static byte[] gen() {
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		cw.visit( 55, ACC_PUBLIC | ACC_SUPER, "org/openj9/test/condy/PrimitiveCondyMethods", null, "java/lang/Object", null );

		MethodVisitor mv;
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "condy_return_string", "()Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitLdcInsn( 
				new ConstantDynamic( "constant_string", "Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"org/openj9/test/condy/BootstrapMethods", 
							"bootstrap_constant_string",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/Class;Ljava/lang/String;)Ljava/lang/Object;"
									).getDescriptor() ),
									"world"
					)
				);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "condy_return_int", "()I", null, null );
			mv.visitCode();
			mv.visitLdcInsn( 
				new ConstantDynamic( "constant_int", "I",
					new Handle(
							H_INVOKESTATIC, 
							"org/openj9/test/condy/BootstrapMethods", 
							"bootstrap_constant_int",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/Class;I)I"
									).getDescriptor() ),
									123432
					)
				);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "condy_return_float", "()F", null, null );
			mv.visitCode();
			mv.visitLdcInsn( 
				new ConstantDynamic( "constant_float", "F",
					new Handle(
							H_INVOKESTATIC, 
							"org/openj9/test/condy/BootstrapMethods", 
							"bootstrap_constant_float",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/Class;F)F"
									).getDescriptor() ),
									10.12F
					)
				);
			mv.visitInsn( FRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}

      {
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "condy_return_long", "()J", null, null );
			mv.visitCode();
			mv.visitLdcInsn( 
				new ConstantDynamic( "constant_long", "J",
					new Handle(
							H_INVOKESTATIC, 
							"org/openj9/test/condy/BootstrapMethods", 
							"bootstrap_constant_long",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/Class;J)J"
									).getDescriptor() ),
									100000000000L
					)
				);
			mv.visitInsn( LRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
      }

      {
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "condy_return_double", "()D", null, null );
			mv.visitCode();
			mv.visitLdcInsn( 
				new ConstantDynamic( "constant_double", "D",
					new Handle(
							H_INVOKESTATIC, 
							"org/openj9/test/condy/BootstrapMethods", 
							"bootstrap_constant_double",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/Class;D)D"
									).getDescriptor() ),
									1111111.12D
					)
				);
			mv.visitInsn( DRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
      }

		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "condy_return_null", "()Ljava/lang/Object;", null, null );
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
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}

		cw.visitEnd();
		return cw.toByteArray();
	}

	public static void main(String[] args) throws Throwable {
		FileOutputStream fos = new FileOutputStream( "PrimitiveCondyMethods.class" );
		fos.write(gen());
		fos.flush();
		fos.close();
	}
}
