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
package com.ibm.jvmti.tests.BCIWithASM;
import static org.objectweb.asm.Opcodes.ACC_PRIVATE;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;
import static org.objectweb.asm.Opcodes.ACONST_NULL;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.ASTORE;
import static org.objectweb.asm.Opcodes.BIPUSH;
import static org.objectweb.asm.Opcodes.DDIV;
import static org.objectweb.asm.Opcodes.DLOAD;
import static org.objectweb.asm.Opcodes.DMUL;
import static org.objectweb.asm.Opcodes.DRETURN;
import static org.objectweb.asm.Opcodes.DSTORE;
import static org.objectweb.asm.Opcodes.DUP;
import static org.objectweb.asm.Opcodes.GETSTATIC;
import static org.objectweb.asm.Opcodes.GOTO;
import static org.objectweb.asm.Opcodes.IADD;
import static org.objectweb.asm.Opcodes.IALOAD;
import static org.objectweb.asm.Opcodes.IASTORE;
import static org.objectweb.asm.Opcodes.ICONST_0;
import static org.objectweb.asm.Opcodes.ICONST_1;
import static org.objectweb.asm.Opcodes.ICONST_2;
import static org.objectweb.asm.Opcodes.ICONST_3;
import static org.objectweb.asm.Opcodes.ICONST_5;
import static org.objectweb.asm.Opcodes.ICONST_M1;
import static org.objectweb.asm.Opcodes.IDIV;
import static org.objectweb.asm.Opcodes.IF_ICMPGE;
import static org.objectweb.asm.Opcodes.IF_ICMPLE;
import static org.objectweb.asm.Opcodes.IF_ICMPLT;
import static org.objectweb.asm.Opcodes.IF_ICMPNE;
import static org.objectweb.asm.Opcodes.ILOAD;
import static org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.IRETURN;
import static org.objectweb.asm.Opcodes.ISTORE;
import static org.objectweb.asm.Opcodes.ISUB;
import static org.objectweb.asm.Opcodes.JSR;
import static org.objectweb.asm.Opcodes.NEWARRAY;
import static org.objectweb.asm.Opcodes.POP;
import static org.objectweb.asm.Opcodes.RET;
import static org.objectweb.asm.Opcodes.RETURN;
import static org.objectweb.asm.Opcodes.T_INT;
import static org.objectweb.asm.Opcodes.V1_6;
import static org.objectweb.asm.Opcodes.V1_7;
import static org.objectweb.asm.Opcodes.V1_5;

import java.io.FileOutputStream;
import java.io.IOException;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;

public class ASMTransformer {

	public static byte[] trasnform_injectNPELogic( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects code that will cause a NPE */
		/*target source : */
		/*public class Source {
				public int method1() {
					String str = null;  <-- injected 
					str.equals("a");	<-- injected 
					return 1;
				}
			}
		 * */
		{
			mv = cw.visitMethod( ACC_PUBLIC, "returnOne", "()I", null, null );
			mv.visitCode();
			Label L0 = new Label();
			mv.visitLabel( L0 );
			mv.visitLineNumber( 5, L0 );
			mv.visitInsn( ACONST_NULL );
			mv.visitVarInsn( ASTORE, 1 );
			
			Label L1 = new Label();
			mv.visitLabel( L1 );
			mv.visitLineNumber( 6, L1 );
			mv.visitVarInsn( ALOAD, 1 );
			mv.visitLdcInsn( "a" );
			mv.visitMethodInsn( INVOKEVIRTUAL, "java/lang/String", "equals", "(Ljava/lang/Object;)Z" );
			mv.visitInsn( POP );
			
			Label L2 = new Label();
			mv.visitLabel( L2 );
			mv.visitLineNumber( 7, L2 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IRETURN );
			
			Label L3 = new Label();
			mv.visitLabel( L3 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source;", null, L0, L3, 0 );
			mv.visitLocalVariable( "str", "Ljava/lang/String;", null, L1, L3, 1 );
			
			mv.visitMaxs( 2, 2 );
			mv.visitEnd();
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] trasnform_injectNewIfBlock( Class classToTransform ) {

		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source2", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source2;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects a new if block 
		target source : 
		public class Source2 {
			public int process( int a ) {
				if ( a == 1 ) {
					return a + 1; 
				} 
				
				if ( a == 2 ) {  |
					return a -1; |<--- this block is injected  
				}  				 |
				
				else {
					return a + 3;
				}
			}
		} */
		{
			Label L0 = new Label();
			Label L1 = new Label ();
			Label L2 = new Label();
			Label L3 = new Label ();
			Label L4 = new Label();
			Label L5 = new Label();
			
			mv = cw.visitMethod( ACC_PUBLIC, "process", "(I)I", null, null );
			mv.visitCode();
			
			mv.visitLabel( L0 );
			mv.visitLineNumber( 6, L0 );	
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( ICONST_1 );
			mv.visitJumpInsn( IF_ICMPNE, L1 );
			
			mv.visitLabel( L2 );
			mv.visitLineNumber( 7, L2 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IADD );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L1 );
			mv.visitLineNumber( 10, L1 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( ICONST_2 );
			mv.visitJumpInsn( IF_ICMPNE, L3 );
			
			mv.visitLabel( L4 );
			mv.visitLineNumber( 11, L4 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( ISUB );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L3 );
			mv.visitLineNumber( 15, L3 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( ICONST_3 );
			mv.visitInsn( IADD );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L5 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source2;", null, L0, L5, 0 );
			mv.visitLocalVariable( "a", "I", null, L0, L5, 1 );
			
			mv.visitMaxs( 2, 2 );
			mv.visitEnd();
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	public static byte[] trasnform_injectCatchAndThrowNewAIOBE( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source3", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source3;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects a try-catch, where the catch block throws a new exception */
		/*target source : */
		/*public class Source3 {
			public int returnOne() {
				try {						   <------------injected 
					String str = null;  
					str.equals("a");	
					return 1;
				} catch ( NullPointerException e ) {    <---injected
					int intArray [] = new int [] {1};	<---injected
					return intArray[1];					<---injected
				}
			}
		}
		 * */
		{
			mv = cw.visitMethod( ACC_PUBLIC, "returnOne", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			
			mv.visitTryCatchBlock( L0, L1, L2, "java/lang/NullPointerException");
			
			mv.visitLabel( L0 );
			mv.visitLineNumber( 6, L0 );
			mv.visitInsn( ACONST_NULL );
			mv.visitVarInsn( ASTORE, 1 );
			
			mv.visitLabel( L3 );
			mv.visitLineNumber( 7, L3 );
			mv.visitVarInsn( ALOAD, 1 );
			mv.visitLdcInsn( "a" );
			mv.visitMethodInsn( INVOKEVIRTUAL, "java/lang/String", "equals", "(Ljava/lang/Object;)Z" );
			mv.visitInsn( POP );
			
			mv.visitLabel( L1 );
			mv.visitLineNumber( 8, L1 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L2 );
			mv.visitLineNumber( 9, L2 );
			mv.visitVarInsn( ASTORE, 1 );
			
			mv.visitLabel( L4 );
			mv.visitLineNumber( 10, L4 );
			mv.visitInsn( ICONST_1 );
			mv.visitVarInsn( NEWARRAY, T_INT );
			mv.visitInsn( DUP );
			mv.visitInsn( ICONST_0 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IASTORE );
			mv.visitVarInsn( ASTORE, 2 );
			
			mv.visitLabel( L5 ); 
			mv.visitLineNumber( 11, L5 );
			mv.visitVarInsn( ALOAD, 2 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IALOAD );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L6 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source3;", null, L0, L6, 0 );
			mv.visitLocalVariable( "str", "Ljava/lang/String;", null, L3, L2, 1 );
			mv.visitLocalVariable( "e", "Ljava/lang/NullPointerException;", null, L4, L6, 1 ); 
			mv.visitLocalVariable( "intArray", "[I", null, L5, L6, 2 ); 
			
			mv.visitMaxs( 4, 3 );
			mv.visitEnd();
		}
	
		cw.visitEnd();
		return cw.toByteArray();
		
	}
	
	public static byte[] trasnform_injectCatchAndThrowNewAIOBE_catchAIOBEAndThrowNewAE( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source4", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source4;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects 2 try-catches, where the inner catch block throws a new exception 
		 * which is caught by the outer catch block, which then throws another new exception.*/
		
		/*target source : */
		/*public class Source4 {
			public int returnOne() {
				try {							<----injected code
					String str = null;  
					str.equals("a");	
					return 1;
				} catch ( NullPointerException e1 ) {					<----injected code
					try {												<----injected code
						int intArray [] = new int [] {1};   			<----injected code
						return intArray[1];								<----injected code
					} catch ( ArrayIndexOutOfBoundsException e2 ) {		<----injected code
						return 1/0;										<----injected code
					}
				}
			}
		}
		 * */
		{
			mv = cw.visitMethod( ACC_PUBLIC, "returnOne", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			Label L7 = new Label();
			Label L8 = new Label();
			Label L9 = new Label();
			
			mv.visitTryCatchBlock( L0, L1, L2, "java/lang/NullPointerException" );
			mv.visitTryCatchBlock( L3, L4, L5, "java/lang/ArrayIndexOutOfBoundsException" );
			
			mv.visitLabel( L0 );
			mv.visitLineNumber( 6, L0 );
			mv.visitInsn( ACONST_NULL );
			mv.visitVarInsn( ASTORE, 1 );
			
			mv.visitLabel( L6 );
			mv.visitLineNumber( 7, L6 );
			mv.visitVarInsn( ALOAD, 1 );
			mv.visitLdcInsn( "a" );
			mv.visitMethodInsn( INVOKEVIRTUAL, "java/lang/String", "equals", "(Ljava/lang/Object;)Z" );
			mv.visitInsn( POP );
			
			mv.visitLabel( L1 );
			mv.visitLineNumber( 8, L1 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L2 );
			mv.visitLineNumber( 9, L2 );
			mv.visitVarInsn( ASTORE, 1 );
			
			mv.visitLabel( L3 );
			mv.visitLineNumber( 11, L3 );
			mv.visitInsn( ICONST_1 );
			mv.visitVarInsn( NEWARRAY, T_INT );
			mv.visitInsn( DUP );
			mv.visitInsn( ICONST_0 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IASTORE );
			mv.visitVarInsn( ASTORE, 2 );
			
			mv.visitLabel( L7 );
			mv.visitLineNumber( 12, L7 );
			mv.visitVarInsn( ALOAD, 2 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IALOAD );
			
			mv.visitLabel( L4 ); 
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L5 );
			mv.visitLineNumber( 13, L5 );
			mv.visitVarInsn( ASTORE, 2 );
			
			mv.visitLabel( L8 );
			mv.visitLineNumber( 14, L8 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( ICONST_0 );
			mv.visitInsn( IDIV );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L9 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source4;", null, L0, L9, 0 );
			mv.visitLocalVariable( "str", "Ljava/lang/String;", null, L6, L2, 1 );
			mv.visitLocalVariable( "e1", "Ljava/lang/NullPointerException;", null, L3, L9, 1 ); 
			mv.visitLocalVariable( "intArray", "[I", null, L7, L5, 2 ); 
			mv.visitLocalVariable( "e2", "Ljava/lang/ArrayIndexOutOfBoundsException;", null, L8, L9, 2 );
			
			mv.visitMaxs( 4, 3 );
			mv.visitEnd();
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	public static byte[] trasnform_inject_Catch2CatchJump( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source5", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source5;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects an outer try-catch, and in the inner catch block, 
		 * injects a GOTO to the outer catch block*/
		 
		/*target source : */
		/*public class Source5 {
			public int returnOne() {
				try {										<--injected
					try {
						return 1/0;
					} catch ( ArithmeticException e ) {
						<GOTO outer catch block>			<--injected
					}
				} catch ( Exception e ) {					<--injected
					return -1;								<--injected
				}
			}
		}
		 * */
		{
			mv = cw.visitMethod( ACC_PUBLIC, "returnOne", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			
			mv.visitTryCatchBlock( L0, L1, L2, "java/lang/ArithmeticException");
			mv.visitTryCatchBlock( L0, L1, L3, "java/lang/Exception");
			mv.visitTryCatchBlock( L2, L4, L3, "java/lang/Exception");
			
			mv.visitLabel( L0 );
			mv.visitLineNumber( 7, L0 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( ICONST_0 );
			mv.visitInsn( IDIV );
			
			mv.visitLabel( L1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L2 );
			mv.visitLineNumber( 8,  L2 );
			mv.visitVarInsn( ASTORE, 1 );
						
			mv.visitLabel( L4 );
			mv.visitLineNumber( 9, L4 );
			
			// jump to outer catch block 
			mv.visitJumpInsn( GOTO, L5 ); //If L3 is target, J9 crashes (works in Oracle) 
			//mv.visitInsn( ICONST_0 );
			//mv.visitInsn( IRETURN );
					
			mv.visitLabel( L3 );
			
			mv.visitLineNumber( 11, L3 );
			mv.visitVarInsn( ASTORE, 1 );
			
			mv.visitLabel( L5 );
			mv.visitLineNumber( 12, L5 );
			mv.visitInsn( ICONST_M1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L6 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source5;", null, L0, L6, 0 );
			mv.visitLocalVariable( "e", "Ljava/lang/ArithmeticException;", null, L4, L3, 1 ); 
			mv.visitLocalVariable( "e", "Ljava/lang/Exception;", null, L5, L6, 1 ); 
			
			mv.visitMaxs( 2, 2 );
			mv.visitEnd();
		}
	
		cw.visitEnd();
		return cw.toByteArray();
		
	}
	
	public static byte[] trasnform_inject_CatchWithSelfGOTO( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source6", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source6;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects logic in catch block to jump to itself 5 times before returning */
		/*target source : */
		/*public class Source6 {
			public int returnOne() {
				int counter = -1;											<--injected 
				try {
					return 1/0;
				} catch ( ArithmeticException e ) {
					counter = counter + 1;									<--injected
					if ( counter < 5 ) {									<--injected
						<jump to the beginning of the current catch block>  <--injected
					}  
					return -1;												<--injected
				}
			}
		}**/
		{
			mv = cw.visitMethod( ACC_PUBLIC, "returnOne", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			Label L7 = new Label();
			Label L8 = new Label();
			
			mv.visitTryCatchBlock( L0, L1, L2, "java/lang/ArithmeticException");

			mv.visitLabel( L3 );
			mv.visitLineNumber( 5, L3 );
			mv.visitInsn( ICONST_M1 );
			mv.visitVarInsn( ISTORE, 1 );
			
			mv.visitLabel( L0 );
			mv.visitLineNumber( 7, L0 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( ICONST_0 );
			mv.visitInsn( IDIV );
			
			mv.visitLabel( L1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L2 );
			mv.visitLineNumber( 8, L2 );
			mv.visitVarInsn( ASTORE, 2 );
			
			mv.visitLabel( L4 );
			mv.visitLineNumber( 10, L4 );
			mv.visitIincInsn( 1, 1 );
			
			mv.visitLabel( L5 );
			mv.visitLineNumber( 12, L5 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( ICONST_5 );
			mv.visitJumpInsn(IF_ICMPGE, L6 );
			
			mv.visitLabel( L7 );
			mv.visitLineNumber( 13, L7 );
			//print a message first
			mv.visitFieldInsn( GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;" );
			mv.visitLdcInsn( "Jumping to beginning of current catch block");
			mv.visitMethodInsn( INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V" );
			
			mv.visitJumpInsn( GOTO, L4 );	//jump to beginning of the current catch block in a loop 5 times
			
			mv.visitLabel( L6 );
			mv.visitLineNumber( 16, L6 );
			mv.visitInsn( ICONST_M1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L8 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source6;", null, L3, L8, 0 );
			mv.visitLocalVariable ( "counter", "I", null, L0, L8, 1 );
			mv.visitLocalVariable( "e", "Ljava/lang/ArithmeticException;", null, L4, L8, 2 ); 
	
			mv.visitMaxs( 2, 3 );
			mv.visitEnd();
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	public static byte[] trasnform_inject_Catch2CatchJump_NegativeTest( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source8", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source8;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects an outer try-catch, and in the inner catch block, 
		 * injects a GOTO to the outer catch block*/
		 
		/*target source : */
		/*public class Source8 {
			public int returnOne() {
				try {										<--injected
					try {
						return 1/0;
					} catch ( ArithmeticException e ) {
						<GOTO outer catch block>			<--injected
					}
				} catch ( Exception e ) {					<--injected
					return -1;								<--injected
				}
			}
		}
		 * */
		{
			mv = cw.visitMethod( ACC_PUBLIC, "returnOne", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			
			mv.visitTryCatchBlock( L0, L1, L2, "java/lang/ArithmeticException");
			mv.visitTryCatchBlock( L0, L1, L3, "java/lang/Exception");
			mv.visitTryCatchBlock( L2, L4, L3, "java/lang/Exception");
			
			mv.visitLabel( L0 );
			mv.visitLineNumber( 7, L0 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( ICONST_0 );
			mv.visitInsn( IDIV );
			
			mv.visitLabel( L1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L2 );
			mv.visitLineNumber( 8,  L2 );
			mv.visitVarInsn( ASTORE, 1 );
						
			mv.visitLabel( L4 );
			mv.visitLineNumber( 9, L4 );
			
			// jump to outer catch block 
			mv.visitJumpInsn( GOTO, L3 ); //jump to wrong level, L3 will try to pop the exception object but there is none in existence. 
			//mv.visitInsn( ICONST_0 );
			//mv.visitInsn( IRETURN );
					
			mv.visitLabel( L3 );
			
			mv.visitLineNumber( 11, L3 );
			mv.visitVarInsn( ASTORE, 1 );
			
			mv.visitLabel( L5 );
			mv.visitLineNumber( 12, L5 );
			mv.visitInsn( ICONST_M1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L6 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source8;", null, L0, L6, 0 );
			mv.visitLocalVariable( "e", "Ljava/lang/ArithmeticException;", null, L4, L3, 1 ); 
			mv.visitLocalVariable( "e", "Ljava/lang/Exception;", null, L5, L6, 1 ); 
			
			mv.visitMaxs( 2, 2 );
			mv.visitEnd();
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	public static byte[] trasnform_inject_ParallelCatchJump( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source9", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source9;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects a jump from the first catch block to the second*/
		 
		/*target source : */
		/*public class Source5 {
				public int returnOne() {
		
				int returnVal = -1;
				
				try {
					returnVal = 1/0;
				} catch ( ArithmeticException e ) {
					returnVal = 0;		
					<add a GOTO to the next catch block below>
				}
				
				try {
					returnVal = 10;
				} catch ( NullPointerException e ) {
					returnVal = -2;
				}
				
				return returnVal;
			}
		}
		 * */
		{
			mv = cw.visitMethod( ACC_PUBLIC, "returnOne", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			Label L7 = new Label();
			Label L8 = new Label();
			Label L9 = new Label();
			Label L10 = new Label();
		
			mv.visitTryCatchBlock( L0, L1, L2, "java/lang/ArithmeticException" );
			mv.visitTryCatchBlock( L3, L4, L5, "java/lang/NullPointerException" );
			
			mv.visitLabel( L6 );
			mv.visitLineNumber( 6, L6 );
			mv.visitInsn( ICONST_M1 );
			mv.visitVarInsn( ISTORE, 1 );
			
			mv.visitLabel( L0 );
			mv.visitLineNumber( 9, L0 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( ICONST_0 );
			mv.visitInsn( IDIV );
			mv.visitVarInsn( ISTORE, 1 );
			
			mv.visitLabel( L1 );
			mv.visitJumpInsn( GOTO, L3 );
			
			mv.visitLabel( L2 );
			mv.visitLineNumber( 10,  L2 );
			mv.visitVarInsn( ASTORE, 2 );
						
			mv.visitLabel( L7 );
			mv.visitLineNumber( 11, L7 );
			
			mv.visitInsn( ICONST_0 );
			mv.visitVarInsn( ISTORE, 1 ); 
			mv.visitJumpInsn( GOTO, L9 );// jump to next catch block 
			
			mv.visitLabel( L3 );
			mv.visitLineNumber( 15, L3 );
			mv.visitIntInsn( BIPUSH, 10 );
			mv.visitVarInsn( ISTORE, 1 );
			
			mv.visitLabel( L4 );
			mv.visitJumpInsn( GOTO, L8 );
			
			
			mv.visitLabel( L5 );
			mv.visitLineNumber( 16, L5 );
			mv.visitVarInsn( ASTORE, 2 ) ; 
			
			mv.visitLabel( L9 );
			mv.visitLineNumber( 17, L9 );
			mv.visitIntInsn( BIPUSH, -2 );
			mv.visitVarInsn( ISTORE, 1 );
			
			mv.visitLabel( L8 );
			mv.visitLineNumber( 20, L8 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( IRETURN );

			mv.visitLabel( L10 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source9;", null, L6, L10, 0 );
			mv.visitLocalVariable( "returnVal", "I", null, L0, L10, 1);
			mv.visitLocalVariable( "e", "Ljava/lang/ArithmeticException;", null, L7, L3, 2 );
			mv.visitLocalVariable( "e", "Ljava/lang/NullPointerException;", null, L9, L8, 2 ); 
			
			mv.visitMaxs( -1,-2 );
			mv.visitEnd();
		}
	
		cw.visitEnd();
		return cw.toByteArray();
		
	}
	
	public static byte[] trasnform_inject_Loop2Loop_Jump( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source10", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source10;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		/*Following transformer injects a jump from the first for loop block to the second*/
		 
		/*target source : */
		/*public class Source5 {
				public int returnOne() {
					
					int counter = 0 ; 
					
					for ( int i = 0 ; i < 10 ; i++ ) {
						counter++;
							
						if ( i == 5 ) {										<--Injected code 
							System.out.println("Jumping to next loop");		<--Injected code 
							jump to next loop 								<--Injected code 
						}													<--Injected code 
					}
					
					for ( int k = 0 ; k < 5 ; k++ ) {
						counter--;
					}
					
					return counter;
				}
		}
		 * */
		{
			mv = cw.visitMethod( ACC_PUBLIC, "returnOne", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			Label L7 = new Label();
			Label L8 = new Label();
			Label L9 = new Label();
			Label L10 = new Label();
			Label L11 = new Label();
			Label L12 = new Label();
			Label L13 = new Label();
			Label L14 = new Label();
			
			mv.visitLabel( L0 );
			mv.visitInsn( ICONST_0 );
			mv.visitVarInsn( ISTORE, 1 );
			
			mv.visitLabel ( L1 );
			mv.visitInsn( ICONST_0 );
			mv.visitVarInsn( ISTORE, 2 );
			
			mv.visitLabel( L2 );
			mv.visitJumpInsn( GOTO, L3 );
			
			mv.visitLabel( L4 );
			mv.visitIincInsn( 1, 1 );
			
			mv.visitLabel( L5 );
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitInsn( ICONST_5 );
			mv.visitJumpInsn( IF_ICMPNE, L6 );
			
			mv.visitLabel( L7 );
			mv.visitFieldInsn( GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;" );
			mv.visitLdcInsn( "Jumping to a different loop");
			mv.visitMethodInsn( INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V" );
			mv.visitJumpInsn( GOTO, L8 );//jump to another loop
			
			mv.visitLabel( L6 );
			mv.visitIincInsn( 2, 1 );
			
			mv.visitLabel( L3 );
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitIntInsn( BIPUSH, 10 );
			mv.visitJumpInsn( IF_ICMPLT, L4 );
			
			mv.visitLabel( L8 );
			mv.visitInsn( ICONST_0 );
			mv.visitVarInsn( ISTORE, 2 );
			
			mv.visitLabel( L9 );
			mv.visitJumpInsn( GOTO, L10 );
			
			mv.visitLabel( L11 );
			mv.visitIincInsn( 1, -1 );
			
			mv.visitLabel( L12 );
			mv.visitIincInsn( 2, 1 );
			
			mv.visitLabel( L10 );
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitInsn( ICONST_5 );
			mv.visitJumpInsn( IF_ICMPLT, L11 );
			
			mv.visitLabel( L13 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L14 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source10;", null, L10, L14, 0 );
			mv.visitLocalVariable( "counter", "I", null, L1, L14, 1 );
			mv.visitLocalVariable( "i", "I", null, L2, L8, 2 );
			mv.visitLocalVariable( "k", "I", null, L9, L13, 2 );
			
			mv.visitMaxs( 2, 3 );
		}
	
		cw.visitEnd();
		return cw.toByteArray();
		
	}

	public static synchronized byte[] inject_call_to_timerMethod( Class classToTransform ) {
		try {
			ClassReader reader = new  ClassReader(classToTransform.getCanonicalName());
			ClassWriter cw = new ClassWriter( reader, ClassWriter.COMPUTE_FRAMES );
			ClassVisitor visitor = new MyClassVisitor(cw, classToTransform.getCanonicalName() );
	        reader.accept(visitor, ClassReader.EXPAND_FRAMES );
			return cw.toByteArray();
		} catch ( IOException e ) {
			return null;
		}
	}
	
	public static byte[] trasnform_inject_stackValueUsage_After_Branch_Using_MethodCall ( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source14", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source14;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC, "method1", "()D", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			
			mv.visitLabel( L0 );
			mv.visitLdcInsn( 4.0 );
			mv.visitVarInsn( DSTORE, 1 ); // var1 = 4
			
			mv.visitLabel ( L1 );
			mv.visitLdcInsn( 2.0 );
			mv.visitVarInsn( DSTORE, 3 ); //var2 = 2
			
			mv.visitLabel( L2 );
			mv.visitVarInsn( DLOAD, 1 );
			mv.visitVarInsn( DLOAD, 3 );
			mv.visitInsn( DDIV ); // 2 <--keep it on stack 
			
			mv.visitLabel( L3 );
			mv.visitVarInsn( ALOAD, 0 );
			
			mv.visitMethodInsn( INVOKESPECIAL, "com/ibm/jvmti/tests/BCIWithASM/Source14", "method2", "()D" );
			mv.visitVarInsn( DSTORE, 7 ); // r = 20
			
			mv.visitLabel( L4 );
			mv.visitVarInsn( DLOAD, 7 ); // 20
			mv.visitInsn( DDIV ); // (2/ 20)  <--this should use the value (2) pushed before method call
					
			mv.visitLabel( L5 );
			mv.visitInsn( DRETURN ); 
			
			mv.visitLabel( L6 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source14;", null, L0, L6, 0 );
			mv.visitLocalVariable( "localVar11", "I", null, L1, L6, 1 );
			mv.visitLocalVariable( "localVar12", "I", null, L2, L6, 3 );
			mv.visitLocalVariable( "localVar13", "I", null, L3, L6, 5 );
			mv.visitLocalVariable( "r", "D", null, L4, L6, 7 );
			mv.visitLocalVariable( "r2", "D", null, L4, L6, 9 );
			mv.visitMaxs( 4, 11 );
		}
	
		{
			mv = cw.visitMethod( ACC_PRIVATE, "method2", "()D", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();

			mv.visitLabel( L0 );
			mv.visitLdcInsn( 10.0 );
			mv.visitVarInsn( DSTORE, 1 ); //var21=10
			
			mv.visitLabel( L1 );
			mv.visitVarInsn( DLOAD, 1 );
			mv.visitLdcInsn( 2.0 );
			mv.visitInsn( DMUL );
			mv.visitVarInsn( DSTORE, 3 );  //var22=20
			
			mv.visitLabel( L2 );
			mv.visitVarInsn( DLOAD, 3 );
			mv.visitInsn( DRETURN );
			
			mv.visitLabel( L3 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source14;", null, L0, L3, 0 );
			mv.visitLocalVariable( "localVar21", "I", null, L1, L3, 1 );
			mv.visitLocalVariable( "localVar22", "I", null, L2, L3, 3 );
			mv.visitMaxs( 4, 5 );
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] trasnform_inject_stackValueUsage_After_Branch_Using_IfCompare ( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source16", 
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source16;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC, "method1", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			Label L7 = new Label();
			
			mv.visitLabel( L0 );
			mv.visitInsn( ICONST_1 );
			mv.visitVarInsn( ISTORE, 1 ); // var1 = 1
			
			mv.visitLabel ( L1 );
			mv.visitInsn( ICONST_2 );
			mv.visitVarInsn( ISTORE, 2 ); //var2 = 2
			
			mv.visitLabel( L2 );
			mv.visitInsn( ICONST_0 );
			mv.visitVarInsn( ISTORE, 3 ); //var3 = 0
			
			mv.visitLabel( L3 );
			mv.visitVarInsn( ILOAD, 1 ); // push extra value before branch
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitVarInsn( ILOAD,3 );
			mv.visitJumpInsn( IF_ICMPLE,  L4 );
			
			mv.visitLabel( L5 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IADD );
			mv.visitVarInsn( ISTORE, 3 );
			
			mv.visitLabel( L4 );
			mv.visitVarInsn( ILOAD, 3 );
			mv.visitInsn( IADD );
			mv.visitVarInsn( ISTORE, 4 );
			
			mv.visitLabel( L6 );
			mv.visitVarInsn( ILOAD, 2 ); // use the extra value which was pushed earlier
			mv.visitVarInsn( ILOAD, 4 );
			mv.visitInsn( IADD );
			mv.visitInsn(IRETURN );
			
			mv.visitLabel( L7 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source16;", null, L0, L7, 0 );
			mv.visitLocalVariable( "var1", "I", null, L1, L7, 1 );
			mv.visitLocalVariable( "var2", "I", null, L2, L7, 2 );
			mv.visitLocalVariable( "var3", "I", null, L3, L7, 3 );
			mv.visitLocalVariable( "var4", "I", null, L6, L7, 4 );
			mv.visitMaxs( 0, 0 );
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	public static byte[] trasnform_inject_unverifiable_dead_code ( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_6, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source17", //using class file version 50
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source17;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC, "method1", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			Label L5 = new Label();
			Label L6 = new Label();
			Label L7 = new Label();
			
			mv.visitLabel( L0 );
			mv.visitInsn( ICONST_1 );
			mv.visitVarInsn( ISTORE, 1 ); // var1 = 1
			
			mv.visitLabel ( L1 );
			mv.visitInsn( ICONST_2 );
			mv.visitVarInsn( ISTORE, 2 ); //var2 = 2
			
			mv.visitLabel( L2 );
			mv.visitInsn( ICONST_0 );
			mv.visitVarInsn( ISTORE, 3 ); //var3 = 0
			
			mv.visitLabel( L3 );
			mv.visitVarInsn( ILOAD, 1 ); 
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitJumpInsn( GOTO,  L4 );
			
			//dead code 
			mv.visitLabel( L5 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitInsn( ICONST_1 );
			mv.visitInsn( IADD );
			mv.visitVarInsn( ISTORE, 3 );
			//dead code
			
			mv.visitLabel( L4 );
			mv.visitVarInsn( ILOAD, 3 );
			mv.visitJumpInsn( GOTO, L6 );
			
			//dead code
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitInsn( IADD );
			mv.visitVarInsn( ISTORE, 4 );
			//dead code
			
			mv.visitLabel( L6 );
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitInsn( IADD );
			mv.visitVarInsn( ISTORE, 4 );
			mv.visitVarInsn( ILOAD, 4 );
			mv.visitInsn(IRETURN );
			
			mv.visitLabel( L7 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source17;", null, L0, L7, 0 );
			mv.visitLocalVariable( "var1", "I", null, L1, L7, 1 );
			mv.visitLocalVariable( "var2", "I", null, L2, L7, 2 );
			mv.visitLocalVariable( "var3", "I", null, L3, L7, 3 );
			mv.visitLocalVariable( "var4", "I", null, L6, L7, 4 );
			mv.visitMaxs( 3, 5 );
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	public static byte[] trasnform_inject_unclean_return ( Class classToTransform ) {
		
		ClassWriter cw = new ClassWriter( ClassWriter.COMPUTE_FRAMES );
		
		cw.visit( V1_7, ACC_PUBLIC + ACC_SUPER, "com/ibm/jvmti/tests/BCIWithASM/Source18", //using class file version 51
				null, "java/lang/Object", null );
		
		cw.visitSource(null, null);
		
		MethodVisitor mv;
		
		//constructor
		{
		     mv = cw.visitMethod( ACC_PUBLIC, "<init>", "()V", null, null );
		     mv.visitCode();
		     Label L0 = new Label();
		     mv.visitLabel( L0 );
		     mv.visitLineNumber( 3, L0 );
		     mv.visitVarInsn( ALOAD, 0 );
		     mv.visitMethodInsn( INVOKESPECIAL, "java/lang/Object", "<init>", "()V" );
		     mv.visitInsn( RETURN );
		     
		     Label L1 = new Label();
		     mv.visitLabel(L1);
		     mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source18;", null , L0, L1, 0 );
		     mv.visitMaxs( 1, 1 );
		     mv.visitEnd();
		}

		{
			mv = cw.visitMethod( ACC_PUBLIC, "method1", "()I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			Label L2 = new Label();
			Label L3 = new Label();
			Label L4 = new Label();
			
			mv.visitLabel( L0 );
			mv.visitInsn( ICONST_1 );
			mv.visitVarInsn( ISTORE, 1 ); // var1 = 1
			
			mv.visitLabel ( L1 );
			mv.visitInsn( ICONST_2 );
			mv.visitVarInsn( ISTORE, 2 ); //var2 = 2
			
			mv.visitLabel( L2 );
			mv.visitVarInsn( ALOAD, 0 );
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitVarInsn( ILOAD, 2 );
			mv.visitMethodInsn(INVOKESPECIAL, "com/ibm/jvmti/tests/BCIWithASM/Source18","add", "(II)I" );
			mv.visitVarInsn( ISTORE, 3 );
			
			mv.visitLabel( L3 );
			mv.visitVarInsn( ILOAD, 3 ); 
			mv.visitInsn(IRETURN );
			
			mv.visitLabel( L4 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source18;", null, L0, L4, 0 );
			mv.visitLocalVariable( "var1", "I", null, L1, L4, 1 );
			mv.visitLocalVariable( "var2", "I", null, L2, L4, 2 );
			mv.visitLocalVariable( "var3", "I", null, L3, L4, 3 );
			mv.visitMaxs(3,4);
		}
		
		{
			mv = cw.visitMethod( ACC_PRIVATE, "add", "(II)I", null, null );
			mv.visitCode();
			
			Label L0 = new Label();
			Label L1 = new Label();
			
			mv.visitLabel( L0 );
			
			mv.visitVarInsn( ILOAD, 1 );
			mv.visitVarInsn( ILOAD, 2 );

			mv.visitInsn( IADD );
			mv.visitInsn( ICONST_5 ); // push extra value onto stack (unclean return)
			mv.visitInsn( IRETURN );
			
			mv.visitLabel( L1 );
			mv.visitLocalVariable( "this", "Lcom/ibm/jvmti/tests/BCIWithASM/Source18;", null, L0, L1, 0 );
			mv.visitLocalVariable( "var1", "I", null, L0, L1, 1 );
			mv.visitLocalVariable( "var2", "I", null, L0, L1, 2 );
			mv.visitMaxs(2,3);
		}
	
		cw.visitEnd();
		return cw.toByteArray();
	}
}
