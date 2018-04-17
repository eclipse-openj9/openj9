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
package org.openj9.test.jsr335.defendersupersends.asm;

import java.util.Iterator;
import java.util.LinkedList;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

import org.testng.log4testng.Logger;

public class AsmTestcaseGenerator {
	private final AsmLoader target;
	public static final String pckg = "org/openj9/test/jsr335/defendersupersends/asm";
	private final String tests[];
	private static Logger logger = Logger.getLogger(AsmTestcaseGenerator.class);
	
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
		
		// Linkage classes are in the form Tester<NAME>, they implement TestHarness and creates a new instance of the 'letter' classes defined above and call out to m()
		String harnessInterface[] = new String[]{"TestHarness"};
		//						Test type		Type		Test name		Interfaces				Test target
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterS",		harnessInterface,		"S"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterT_K",	harnessInterface,		"T_K"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterT_L",	harnessInterface,		"T_L"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterU",		harnessInterface,		"U"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterV_K",	harnessInterface,		"V_K"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterV_L",	harnessInterface,		"V_L"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterV_M",	harnessInterface,		"V_M"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterW_L",	harnessInterface,		"W_L"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterW_M",	harnessInterface,		"W_M"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterW_O",	harnessInterface,		"W_O"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterX_L",	harnessInterface,		"X_L"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterX_M",	harnessInterface,		"X_M"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterX_N",	harnessInterface,		"X_N"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterY_M",	harnessInterface,		"Y_M"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterY_N",	harnessInterface,		"Y_N"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterZ",		harnessInterface,		"Z"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterO1",		harnessInterface,		"O1"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterO2",		harnessInterface,		"O2"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterO3",		harnessInterface,		"O3"));
		target.addAsmGenerator(new HarnessAsm(Type.CLASS,	"TesterO4",		harnessInterface,		"O4"));
		
		// The following are generated testcases for JUnit, they create a new Linkage class and call m() looking for an expected result:
		JUnitTestcase testcase = new JUnitTestcase("TestDefenderSupersends");
		testcase.newTestcase('A', "testO1", "TesterO1");
		testcase.newTestcase('F', "testO2", "TesterO2");
		testcase.newTestcase('G', "testO3", "TesterO3");
		testcase.newTestcase(NoSuchMethodError.class, "testO4", "TesterO4");
		testcase.newTestcase('P', "testS", "TesterS");
		testcase.newTestcase(IncompatibleClassChangeError.class, "testTK", "TesterT_K");
		testcase.newTestcase('L', "testTL", "TesterT_L");
		testcase.newTestcase(IncompatibleClassChangeError.class, "testU", "TesterU");
		testcase.newTestcase(IncompatibleClassChangeError.class, "testVK", "TesterV_K");
		testcase.newTestcase('L', "testVL", "TesterV_L");
		testcase.newTestcase('B', "testVM", "TesterV_M");
		testcase.newTestcase('L', "testWL", "TesterW_L");
		testcase.newTestcase('B', "testWM", "TesterW_M");
		testcase.newTestcase('D', "testWO", "TesterW_O");
		testcase.newTestcase('L', "testXL", "TesterX_L");
		testcase.newTestcase('B', "testXM", "TesterX_M");
		testcase.newTestcase('B', "testXN", "TesterX_N");
		testcase.newTestcase('B', "testYM", "TesterY_M");
		testcase.newTestcase('B', "testYN", "TesterY_N");
		testcase.newTestcase('D', "testZ", "TesterZ");
		
		target.addAsmGenerator(testcase);
		
		try {
			return target.findClass(testcase.name());
		} catch (ClassNotFoundException e) {
			logger.error("FAILED");
			e.printStackTrace();
			return null;
		}
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
	
	/**
	 * Generate a class that adapts our web of AsmLoaders to the interface "TestHarness", which provides two methods: m and w.
	 * The w method exists to go through the invocation path and class load everything (since classes can be loaded from disk),
	 * and then 'm' allocates the class to execute and executes every single function in tests[] to time how long it takes to
	 * resolve all the invocations. Since each chain is unique and doesn't get called twice, it's an accurate gauge to how long
	 * that takes. If test is "a", "b", "c", "d", then m() would look like:
	 * 	char m(){
	 * 		TargetClass t = new TargetClass();
	 * 		t.a();
	 * 		t.b();
	 * 		t.c();
	 * 		return t.d();
	 * 	}
	 * The result from the LAST method is returned.	
	 * 
	 * When doing a sanity test, w() is not used and m() serves as a passthrough to the actual test.
	 *
	 */
	class HarnessAsm implements AsmGenerator {
		public HarnessAsm(Type type, String name, String interfaces[], String target) {
			this.type = type;
			this.name = name;
			if (interfaces != null) {
				this.interfaces = interfaces;
			} else {
				this.interfaces = new String[0];
			}
			this.target = target;
		}
		
		private final Type type;
		private final String name;
		private final String interfaces[];
		private final String target;

		@Override
		public byte[] getClassData() {
			ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_MAXS);
			String superInterfaces[] = null;
			if (interfaces.length > 0) {
				superInterfaces = new String[interfaces.length];
				for (int i = 0; i < superInterfaces.length; i++) {
					superInterfaces[i] = pckg +"/"+interfaces[i];
				}
			}
			int mods = 0;
			if (type == Type.INTERFACE) {
				throw new InternalError("Type.INTERFACE incompatible with HarnessAsm");
			} else {
				mods = ACC_SUPER;
			}
			cw.visit(V1_8, ACC_PUBLIC + mods, pckg +"/"+name, null, "java/lang/Object", superInterfaces);
					
			/* Generate a constructor */
			MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
			
			String targetSend = "";
			if (target.contains("/")) {
				targetSend = target;
			} else {
				targetSend = pckg+"/"+target; 
			}
			
			/* Generate an m() */
			mv = cw.visitMethod(ACC_PUBLIC, "m", "()C", null, new String[] { "java/lang/Throwable" });
			mv.visitCode();
			/* It allocates a new object of type "targetSend" */
			mv.visitTypeInsn(NEW, targetSend);
			mv.visitInsn(DUP);
			mv.visitMethodInsn(INVOKESPECIAL, targetSend, "<init>", "()V");
			mv.visitVarInsn(ASTORE, 1);
			/* Call out to every function in test[] */
			for (int i = 0; i < tests.length; i++) {
				mv.visitVarInsn(ALOAD, 1);
				mv.visitMethodInsn(INVOKEVIRTUAL, targetSend, tests[i], "()C");
				mv.visitVarInsn(ISTORE, 2);
			}
			/* Only the last function's result is on the stack */
			mv.visitVarInsn(ILOAD, 2);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
			
			/* Generate the warmup function, identical except it only calls out to w() */
			mv = cw.visitMethod(ACC_PUBLIC, "w", "()C", null, new String[] { "java/lang/Throwable" });
			mv.visitCode();
			mv.visitTypeInsn(NEW, targetSend);
			mv.visitInsn(DUP);
			mv.visitMethodInsn(INVOKESPECIAL, targetSend, "<init>", "()V");
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitMethodInsn(INVOKEVIRTUAL, targetSend, "w", "()C");
			mv.visitInsn(IRETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
			
			
			cw.visitEnd();
			return cw.toByteArray();
		}

		@Override
		public String name() {
			return pckg.replace('/', '.')+'.'+name;
		}
		
		public String asmName() {
			return pckg+"/"+name;
		}

	}
	
	/**
	 * Generate a JUnit test case class that we can run the tests on
	 *
	 */
	class JUnitTestcase implements AsmGenerator {
		public JUnitTestcase(String className) {
			this.name = className;
		}
		
		private final String name;
		private final LinkedList<JUnitTestcaseTest> testCases = new LinkedList<JUnitTestcaseTest>();
		
		@Override
		public byte[] getClassData() {
			ClassWriter cw = new ClassWriter(0);
			MethodVisitor mv;

			/* Class setup and constructor */
			cw.visit(V1_8, ACC_PUBLIC + ACC_SUPER, asmName(), null, "junit/framework/TestCase", null);
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "junit/framework/TestCase", "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
			
			Iterator<JUnitTestcaseTest> itr = testCases.iterator();
			
			/* Insert each test case */
			while (itr.hasNext()) {
				JUnitTestcaseTest jutt = itr.next();
				jutt.generate(cw);
			}
			
			return cw.toByteArray();
		}
		
		/**
		 * Generate a new testcase called 'name' that does a supercall to 'target' and expects 'expected' to be returned
		 * @param expected Expected return value from target target.m()
		 * @param name Name of the function
		 * @param target Name of the class to target for the invocation
		 */
		public void newTestcase(char expected, String name, String target) {
			testCases.add(new JUnitTestcaseTest(expected, name, target));
		}
		
		/**
		 * Generate a new testcase called 'name' that does a supercall to 'target' and expects an exception to be thrown
		 * @param expected Expected exception class to be thrown
		 * @param name Name of the function
		 * @param target Name of the class to target for the invocation
		 */
		public void newTestcase(Class<?> expected, String name, String target) {
			testCases.add(new JUnitTestcaseTest(expected, name, target));
		}

		@Override
		public String name() {
			return pckg.replace('/', '.')+'.'+name;
		}
		
		class JUnitTestcaseTest{
			public JUnitTestcaseTest(char expected, String name, String target) {
				this.expectedReturnValue = expected;
				this.name = name;
				this.callTo = target;
				this.expectedExceptionType = null;
			}
			
			public JUnitTestcaseTest(Class<?> expectedException, String name, String target) {
				this.expectedExceptionType = expectedException;
				this.name = name;
				this.callTo = target;
				this.expectedReturnValue = 0;
			}
			
			private final char expectedReturnValue;
			private final Class<?> expectedExceptionType;
			private final String name;
			private final String callTo;
			
			public void generate(ClassWriter cw) {
				MethodVisitor mv;
				/* If expecting an exception thrown */
				/* Looks like:
				 * 	void name(){
				 * 		try {
				 * 			TestHarness t = AsmUtils.newInstance(target);
				 * 			char r = t.m();
				 * 			fail("No exception thrown, returned '"+r+"'");
				 * 		} catch(ExceptionType e) {
				 * 		}
				 * 	}
				 */
				if (expectedExceptionType != null) {
					String expectedExceptionTypeString = expectedExceptionType.getName().replace('.', '/');
					mv = cw.visitMethod(ACC_PUBLIC, name, "()V", null, new String[] { "java/lang/Throwable" });
					mv.visitCode();
					Label l0 = new Label();
					Label l1 = new Label();
					Label l2 = new Label();
					mv.visitTryCatchBlock(l0, l1, l2, expectedExceptionTypeString);
					mv.visitLabel(l0);
					mv.visitLdcInsn(callTo);
					mv.visitMethodInsn(INVOKESTATIC, AsmUtils.class.getName().replace('.', '/'), "newInstance", "(Ljava/lang/String;)Ljava/lang/Object;");
					mv.visitTypeInsn(CHECKCAST, TestHarness.class.getName().replace('.', '/'));
					mv.visitVarInsn(ASTORE, 1);
					mv.visitVarInsn(ALOAD, 1);
					mv.visitMethodInsn(INVOKEINTERFACE, TestHarness.class.getName().replace('.', '/'), "m", "()C");
					mv.visitVarInsn(ISTORE, 2);
					mv.visitTypeInsn(NEW, "java/lang/StringBuilder");
					mv.visitInsn(DUP);
					mv.visitLdcInsn("No exception thrown, returned '");
					mv.visitMethodInsn(INVOKESPECIAL, "java/lang/StringBuilder", "<init>", "(Ljava/lang/String;)V");
					mv.visitVarInsn(ILOAD, 2);
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/StringBuilder", "append", "(C)Ljava/lang/StringBuilder;");
					mv.visitLdcInsn("'");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/StringBuilder", "append", "(Ljava/lang/String;)Ljava/lang/StringBuilder;");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/StringBuilder", "toString", "()Ljava/lang/String;");
					mv.visitMethodInsn(INVOKESTATIC, asmName(), "fail", "(Ljava/lang/String;)V");
					mv.visitLabel(l1);
					Label l3 = new Label();
					mv.visitJumpInsn(GOTO, l3);
					mv.visitLabel(l2);
					mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {expectedExceptionTypeString});
					mv.visitVarInsn(ASTORE, 1);
					mv.visitLabel(l3);
					mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
					mv.visitInsn(RETURN);
					mv.visitMaxs(3, 3);
					mv.visitEnd();
					/*
					 * 	void name(){
					 * 		Target t = new Target();
					 * 		assertEquals(expectedValue, t.m());
					 * 	}
					 */
				} else {
					mv = cw.visitMethod(ACC_PUBLIC, name, "()V", null, new String[] { "java/lang/Throwable" });
					mv.visitCode();
					mv.visitLdcInsn(callTo);
					mv.visitMethodInsn(INVOKESTATIC, AsmUtils.class.getName().replace('.', '/'), "newInstance", "(Ljava/lang/String;)Ljava/lang/Object;", false);
					mv.visitTypeInsn(CHECKCAST, TestHarness.class.getName().replace('.', '/'));
					mv.visitVarInsn(ASTORE, 1);
					mv.visitVarInsn(ALOAD, 1);
					mv.visitMethodInsn(INVOKEINTERFACE, TestHarness.class.getName().replace('.', '/'), "m", "()C", true);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitIntInsn(BIPUSH, expectedReturnValue);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitMethodInsn(INVOKESTATIC, JUnitTestcase.this.asmName(), "assertEquals", "(CC)V", false);
					mv.visitInsn(RETURN);
					mv.visitMaxs(2, 3);
					mv.visitEnd();
				}
			}
		}

		@Override
		public String asmName() {
			return pckg+"/"+name;
		}
	}	
	
}
