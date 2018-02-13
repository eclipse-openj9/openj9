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
package org.openj9.test.reflect.defendersupersends.asm;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;

public class AsmTestcaseGenerator {
	private final AsmLoader target;
	public static final String pckg = "org/openj9/test/reflect/defendersupersends/asm";
	private final String tests[];
	
	public AsmTestcaseGenerator(AsmLoader cl, String[] tests) {
		target = cl;
		this.tests = tests;
	}
	
	enum Type {
		CLASS,
		INTERFACE
	}
	
	public Class<?> getDefenderSupersendTestcase() {
		// Top level interfaces. They do not implement or extend any other interfaces.
		//						Test type			Type				Name	Super interfaces
		target.addAsmGenerator(newAsmWithReturnCode(Type.INTERFACE, 	"A", 	new String[]{}));
		target.addAsmGenerator(newAsmWithReturnCode(Type.INTERFACE, 	"B", 	new String[]{}));
		target.addAsmGenerator(newAsm(				Type.INTERFACE, 	"C", 	new String[]{}));
		target.addAsmGenerator(newAsmWithReturnCode(Type.INTERFACE, 	"D", 	new String[]{}));		
		
		// Mid Level interfaces. They implement only top level interfaces
		//						Test type			Type				Name	Super interfaces			Target
		target.addAsmGenerator(newAsm(				Type.INTERFACE, 	"E", 	new String[]{"A"}));
		target.addAsmGenerator(newAsmWithReturnCode(Type.INTERFACE, 	"F", 	new String[]{"A"}));
		target.addAsmGenerator(newAsmWithReturnCode(Type.INTERFACE, 	"G", 	new String[]{"C"}));
		target.addAsmGenerator(newAsm(				Type.INTERFACE,		"H", 	new String[]{"C"}));
		target.addAsmGenerator(newAsm(				Type.INTERFACE, 	"K", 	new String[]{"A", "B"}));
		target.addAsmGenerator(newAsmWithReturnCode(Type.INTERFACE, 	"L", 	new String[]{"A", "B"}));
		target.addAsmGenerator(newAsm(				Type.INTERFACE, 	"M", 	new String[]{"B", "C"}));
		target.addAsmGenerator(newAsm(				Type.INTERFACE, 	"N", 	new String[]{"B", "C"}));
		target.addAsmGenerator(newAsmForSupersend(	Type.INTERFACE, 	"O", 	new String[]{"B", "D"},		"D"));
		target.addAsmGenerator(newAsmWithReturnCode(Type.INTERFACE, 	"P", 	new String[]{"B", "C"}));
		
		// Bottom level classes. They are the bottom layer of tests and implement only second layer interfaces
		//						Test type			Type				Name	Super interfaces				Target
		target.addAsmGenerator(newAsm(				Type.CLASS, 		"S", 	new String[]{"P"}));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"T_K", 	new String[]{"K", "L"},			"K"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"T_L", 	new String[]{"K", "L"},			"L"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"U", 	new String[]{"K", "L"},			"O"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"V_K", 	new String[]{"K", "L", "M"},	"K"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"V_L", 	new String[]{"K", "L", "M"},	"L"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"V_M", 	new String[]{"K", "L", "M"},	"M"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"W_L", 	new String[]{"L", "M", "O"},	"L"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"W_M", 	new String[]{"L", "M", "O"},	"M"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"W_O", 	new String[]{"L", "M", "O"},	"O"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"X_L", 	new String[]{"L", "M", "N"},	"L"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"X_M", 	new String[]{"L", "M", "N"},	"M"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"X_N", 	new String[]{"L", "M", "N"},	"N"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"Y_M", 	new String[]{"M", "N"},			"M"));
		target.addAsmGenerator(newAsmForSupersend(	Type.CLASS, 		"Y_N", 	new String[]{"M", "N"},			"N"));
		target.addAsmGenerator(newAsm(				Type.CLASS, 		"Z", 	new String[]{"D"}));
		target.addAsmGenerator(newAsm(				Type.CLASS, 		"O1", 	new String[]{"E"}));
		target.addAsmGenerator(newAsm(				Type.CLASS, 		"O2", 	new String[]{"F"}));
		target.addAsmGenerator(newAsm(				Type.CLASS, 		"O3", 	new String[]{"G"}));
		target.addAsmGenerator(newAsm(				Type.CLASS, 		"O4", 	new String[]{"H"}));
		
		return null;
	}
	
	/**
	 * Generate ASM for the provided name, type, and super classes/interfaces. Do not implement any methods.
	 */
	public AsmGenerator newAsm(Type type, String name, String[] superClasses){
		return new Asm(type, name, superClasses, null, false);
	}
	
	/**
	 * Generate ASM for the provided name, type, and super classes/interfaces. Implement tests (m()) with supersend to target.
	 */
	public AsmGenerator newAsmForSupersend(Type type, String name, String[] superClasses, String target){
		return new Asm(type, name, superClasses, target, false);
	}
	
	/**
	 * Generate ASM for the provided name, type, and super classes/interfaces. Implement tests (m()) with return code.
	 */
	public AsmGenerator newAsmWithReturnCode(Type type, String name, String[] superClasses){
		return new Asm(type, name, superClasses, null, true);
	}
	
	
	class Asm implements AsmGenerator{
		public Asm(Type type, String name, String [] superClasses, String superSend, boolean doRc) {
			this.superClasses = superClasses;
			this.type = type;
			this.name = name;
			this.superSend = superSend;
			this.doRc = doRc;
			if (doRc == true && superSend != null) {
				throw new IllegalArgumentException("While generating class "+name+", you tried to specify BOTH a superSend to \""+superSend+"\" and generate a return code.  Pick ONE or NONE.");
			}
		}
		public String asmName() {
			return pckg+"/"+name;
		}
		
		private final String [] superClasses;
		private final String name;
		private final Type type;
		private final String superSend;
		private final boolean doRc;
		public byte[] getClassData() {
			ClassWriter cw = new ClassWriter(0);
			String longSuperclasses[] = new String[superClasses.length];
			for (int i = 0; i < superClasses.length; i++) {
				longSuperclasses[i] = pckg +"/"+superClasses[i];
			}
			int mods = 0;
			if (type == Type.INTERFACE) {
				mods = ACC_INTERFACE + ACC_ABSTRACT;
			} else {
				mods = ACC_SUPER;
			}
			cw.visit(V1_8, ACC_PUBLIC + mods, pckg +"/"+name, null, "java/lang/Object", longSuperclasses);

			/* Generate Lookup object provider method */
			{
				cw.visitInnerClass("java/lang/invoke/MethodHandles$Lookup", "java/lang/invoke/MethodHandles", "Lookup", ACC_PUBLIC + ACC_FINAL + ACC_STATIC);
				MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "lookup", "()Ljava/lang/invoke/MethodHandles$Lookup;", null, null);
				mv.visitCode();
				mv.visitMethodInsn(INVOKESTATIC, "java/lang/invoke/MethodHandles", "lookup", "()Ljava/lang/invoke/MethodHandles$Lookup;", false);
				mv.visitInsn(ARETURN);
				mv.visitMaxs(1, 0);
				mv.visitEnd();
			}
			
			/* Generate the constructor */
			if (type == Type.CLASS) {
				MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
				mv.visitCode();
				mv.visitVarInsn(ALOAD, 0);
				mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
				mv.visitInsn(RETURN);
				mv.visitMaxs(1, 1);
				mv.visitEnd();
			}
			
			/* Generate the methods (from tests[]) */
			for (int i = 0; i < tests.length; i++) {
				/* If we have a class to supersend to, generate methods from test[] to link them
				 * ie, char f() is implemented as char f(){return Q.f();}
				 */
				if (superSend != null) {
					String targetSend = pckg+"/"+superSend;
					MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, tests[i], "()C", null, null);
					mv.visitCode();
					mv.visitVarInsn(ALOAD, 0);

					/* the targetSend Class is always an Interface in this test case, so the itf flag will be set to true for all tests */
					mv.visitMethodInsn(INVOKESPECIAL, targetSend, tests[i], "()C", true);
					mv.visitInsn(IRETURN);
					mv.visitMaxs(1, 1);
					mv.visitEnd();
				/* If we'd rather generate a return code, generate methods (from test[]) that return the first letter of this class
				 * ie, if the class is Quantum, then the method would look like void f(){return 'Q';}
				 * Using this method, we can determine if the correct method was called
				 */
				} else if (doRc) {
					MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, tests[i], "()C", null, null);
					mv.visitCode();
					mv.visitIntInsn(BIPUSH, name.charAt(0));
					mv.visitInsn(IRETURN);
					mv.visitMaxs(1, 1);
					mv.visitEnd();
				}
			}
			
			cw.visitEnd();
			return cw.toByteArray();
		}

		@Override
		public String name() {
			return pckg.replace('/', '.')+'.'+name;
		}
	}
	
}
