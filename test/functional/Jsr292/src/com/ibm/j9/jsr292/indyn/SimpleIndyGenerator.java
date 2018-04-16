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
import static org.objectweb.asm.Opcodes.H_INVOKESTATIC;
import static org.objectweb.asm.Opcodes.H_INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.ILOAD;
import static org.objectweb.asm.Opcodes.IRETURN;
import static org.objectweb.asm.Opcodes.RETURN;
import static org.objectweb.asm.Opcodes.LRETURN;
import static org.objectweb.asm.Opcodes.V1_7;

import java.io.FileOutputStream;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Type;

public class SimpleIndyGenerator {

	public static void main( String[] args ) throws Throwable {
		FileOutputStream fos = new FileOutputStream( "GenIndyn.class" );
		fos.write( makeExample());
		fos.flush();
		fos.close();
	}

	public static byte[] makeExample() throws Throwable {
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		cw.visit( V1_7, ACC_PUBLIC | ACC_SUPER, "com/ibm/j9/jsr292/indyn/GenIndyn", null, "java/lang/Object", null );

		MethodVisitor mv;
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "indy_return_string", "()Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "constant_string", "()Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_constant_string",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() ),
									"hello",
									"world"
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "box_and_unbox_int", "(I)I", null, null );
			mv.visitCode();
			mv.visitVarInsn( ILOAD, 0 );
			mv.visitInvokeDynamicInsn( "box_and_unbox_int", "(I)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_box_and_unbox_int",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "return_constant_ignore_actual_args", "(IIIIII)I", null, null );
			mv.visitCode();
			mv.visitVarInsn( ILOAD, 0 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitVarInsn( ILOAD, 3 );
			mv.visitVarInsn( ILOAD, 4 );
			mv.visitVarInsn( ILOAD, 5 );
			mv.visitInvokeDynamicInsn( "return_constant_ignore_actual_args", "(IIIIII)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_return_constant_ignore_actual_args",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_to_string", "(Ljava/lang/Object;)Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD, 0 );
			mv.visitInvokeDynamicInsn( "call_to_string", "(Ljava/lang/Object;)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_to_string",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_value_of", "(Ljava/lang/Object;)Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD, 0 );
			mv.visitInvokeDynamicInsn( "call_value_of", "(Ljava/lang/Object;)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_value_of",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_list_size", "(Ljava/util/List;)I", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD, 0 );
			mv.visitInvokeDynamicInsn( "call_list_size", "(Ljava/util/List;)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_list_size",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_tostring_of", "(Ljava/lang/Integer;)Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD,0 );
			mv.visitInvokeDynamicInsn( "call_tostring_of", "(Ljava/lang/Integer;)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_tostring_of",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_to_getter", "(Lcom/ibm/j9/jsr292/indyn/Helper;)I", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD,0 );
			mv.visitInvokeDynamicInsn( "call_to_getter", "(Lcom/ibm/j9/jsr292/indyn/Helper;)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_to_getter",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_to_setter", "(Lcom/ibm/j9/jsr292/indyn/Helper;I)V", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD,0 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInvokeDynamicInsn( "call_to_setter", "(Lcom/ibm/j9/jsr292/indyn/Helper;I)V",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_to_setter",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( RETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_to_staticGetter", "()I", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "call_to_staticGetter", "()I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_to_staticGetter",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_to_staticSetter", "(I)V", null, null );
			mv.visitCode();
			mv.visitVarInsn( ILOAD, 0 );
			mv.visitInvokeDynamicInsn( "call_to_staticSetter", "(I)V",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_to_staticSetter",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( RETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "bind_mh_to_receiver", "()I", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "bind_mh_to_receiver", "()I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_bind_mh_to_receiver",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "filter_return", "(ILjava/lang/String;)I", null, null );
			mv.visitCode();
			mv.visitVarInsn( ILOAD, 0 );
			mv.visitVarInsn( ALOAD, 1 );
			mv.visitInvokeDynamicInsn( "filter_return", "(ILjava/lang/String;)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_filter_return",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
									
					);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "guard_with_test_constant_true", "(I)I", null, null );
			mv.visitCode();
			mv.visitVarInsn( ILOAD, 0 );
			mv.visitInvokeDynamicInsn( "guard_with_test_constant_true", "(I)I",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_guard_with_test_constant_true",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
									
					);
			mv.visitInsn( IRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "guard_with_test_let_input_decide", "(I)Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitVarInsn( ILOAD, 0 );
			mv.visitInvokeDynamicInsn( "guard_with_test_let_input_decide", "(I)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_guard_with_test_let_input_decide",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
									
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "filter_arguments_on_drop_arguments", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD, 0 );
			mv.visitVarInsn( ALOAD, 1 );
			mv.visitVarInsn( ALOAD, 2 );
			mv.visitVarInsn( ALOAD, 3 );
			mv.visitInvokeDynamicInsn( "filter_arguments_on_drop_arguments", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_filter_arguments_on_drop_arguments",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
									
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_invalid_method", "(Ljava/lang/Object;)Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD, 0 );
			mv.visitInvokeDynamicInsn( "call_invalid_method", "(Ljava/lang/Object;)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_invalid_method",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "call_Illegal_crosspackage_method", "(Ljava/lang/Object;)Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD, 0 );
			mv.visitInvokeDynamicInsn( "call_Illegal_crosspackage_method", "(Ljava/lang/Object;)Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_call_Illegal_crosspackage_method",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "invalid_bootstrap", "(Ljava/lang/Object;)Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitVarInsn( ALOAD, 0 );
			mv.visitInvokeDynamicInsn( "invalid_bootstrap", "(Ljava/lang/Object;)Ljava/lang/String;",
					  new Handle(
					  H_INVOKESTATIC, 
					  "com/ibm/j9/jsr292/indyn/BootstrapMethods", 
					  "method_must_not_exist",        // a Method that doesn't exist
					  Type.getType(
							  "(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
							  ).getDescriptor() ) );
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "ldc_toString", "()Ljava/lang/invoke/MethodHandle;", null, null );
			mv.visitCode();
			mv.visitLdcInsn(new Handle(
							H_INVOKEVIRTUAL, 
							"java/lang/Object", 
							"toString",
							Type.getType(
									"()Ljava/lang/String;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "ldc_toLowerCase", "()Ljava/lang/invoke/MethodHandle;", null, null );
			mv.visitCode();
			mv.visitLdcInsn(new Handle(
							H_INVOKEVIRTUAL, 
							"java/lang/String", 
							"toLowerCase",
							Type.getType(
									"()Ljava/lang/String;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "ldc_invalid_MH", "()Ljava/lang/invoke/MethodHandle;", null, null );
			mv.visitCode();
			mv.visitLdcInsn(new Handle(
							H_INVOKEVIRTUAL, 
							"java/lang/String", 
							"invalidMethod",
							Type.getType(
									"()Ljava/lang/String;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "ldc_illegal_private_MH", "()Ljava/lang/invoke/MethodHandle;", null, null );
			mv.visitCode();
			mv.visitLdcInsn(new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/Helper", 
							"staticPrivateMethod",
							Type.getType(
									"()Ljava/lang/String;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "ldc_illegal_crosspackage_MH", "()Ljava/lang/invoke/MethodHandle;", null, null );
			mv.visitCode();
			mv.visitLdcInsn(new Handle(
							H_INVOKESTATIC, 
							"differentpackage/CrossPackageHelper", 
							"staticProtectedMethod",
							Type.getType(
									"()Ljava/lang/String;"
									).getDescriptor() )
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "ldc_String_type", "()Ljava/lang/invoke/MethodType;", null, null );
			mv.visitCode();
			mv.visitLdcInsn(Type.getType("()Ljava/lang/String;"));
					
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "ldc_bootstrapmethod_type", "()Ljava/lang/invoke/MethodType;", null, null );
			mv.visitCode();
			mv.visitLdcInsn(Type.getType("(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"));
					
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "ldc_invalid_type", "()Ljava/lang/invoke/MethodType;", null, null );
			mv.visitCode();
			mv.visitLdcInsn(Type.getType("(I)Ljava/lang/str;"));
					
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "bsm_signature_obj_array", "()Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "bootstrap_objects", "()Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_objects",
							Type.getType(
									"([Ljava/lang/Object;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() ),
									"hello",
									"world"
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "bsm_signature_L_S_MT_obj_array", "()Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "bootstrap_L_S_MT_objects", "()Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_objects",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;[Ljava/lang/Object;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() ),
									"hello",
									"world"
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "bsm_signature_L_S_MT_obj_array_passing_primitives", "()Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "bootstrap_L_S_MT_objects_with_prims", "()Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_objects",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;[Ljava/lang/Object;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() ),
									"hello",
									"world",
									444,
									555,
									666,
									777,
									888
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "bsm_signature_L_S_MT_byte_array", "()Ljava/lang/String;", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "bootstrap_L_S_MT_byte_array", "()Ljava/lang/String;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_byte_array",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;[B)Ljava/lang/invoke/CallSite;"
									).getDescriptor() ),
									(byte)4,
									(byte)5,
									(byte)6,
									(byte)7,
									(byte)8
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "bsm_signature_L_S_MT_sss_long_array", "()J", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "bootstrap_L_S_MT_sss_long_array", "()J",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"bootstrap_sss_long_array",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;SSS[J)Ljava/lang/invoke/CallSite;"
									).getDescriptor() ),
									(short)4,
									(short)5,
									(short)6,
									(long)7,
									(long)8
					);
			mv.visitInsn( LRETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		
		{
			mv = cw.visitMethod( ACC_PUBLIC | ACC_STATIC, "test_boostrap_return_constant_MethodType", "()Ljava/lang/invoke/MethodType;", null, null );
			mv.visitCode();
			mv.visitInvokeDynamicInsn( "boostrap_return_constant_MethodType", "()Ljava/lang/invoke/MethodType;",
					new Handle(
							H_INVOKESTATIC, 
							"com/ibm/j9/jsr292/indyn/BootstrapMethods", 
							"boostrap_return_constant_MethodType",
							Type.getType(
									"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
									).getDescriptor() ),
							Type.getType(
									"(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;"
									)
									
					);
			mv.visitInsn( ARETURN );
			mv.visitMaxs( 0, 0 );
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

}
