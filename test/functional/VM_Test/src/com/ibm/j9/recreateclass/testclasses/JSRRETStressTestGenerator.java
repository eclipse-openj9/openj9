/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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

package com.ibm.j9.recreateclass.testclasses;

import static org.objectweb.asm.Opcodes.*;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;

/**
 * This class generates a class which uses JSR/RET bytecodes to implement finally block.
 * The class file generated is similar to following java class. 
 * However, the bytecodes generated are sub-optimal as there is a JSR bytecode inserted
 * at the end of every case in switch-case block, instead of having a single JSR at the end of the switch-case block.
 * Intention behind this is to allow JSRInliner in J9VM to inline 
 * sub-routine block at multiple places in the switch-case block.
 * 
 *	class JSRRETStressTest {
 *		public boolean method(int arg) {
 *			int val = 0;
 *			boolean res = false;
 *			try {
 *				switch(arg) {
 *				case 0:
 *					val = arg + 0;
 *					break;
 *				case 1:
 *					val = arg + 1;
 *					break;
 *				case 2:
 *					val = arg + 2;
 *					break;
 *				...
 *				< more case blocks here >
 *				...
 *				}
 *			} finally {
 *				try {
 *					switch(val) {
 *					case 0:
 *						val = val - 0;
 *						break;
 *					case 2:
 *						val = val - 1;
 *						break;
 *					case 4:
 *						val = val - 2;
 *						break;
 *					...
 *					< more case blocks here >
 *					...
 *					default:
 *						val = arg - 0;
 *						break;
 *					}
 *				} finally {
 *					res = (val == arg);
 *				}
 *			}
 *			return res;
 *		}
 *	}
 * 
 * @author ashutosh
 */
public class JSRRETStressTestGenerator {
	public static byte[] generateClassData() {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;

		cw.visit(V1_1, ACC_PUBLIC,
				"com/ibm/j9/recreateclasscompare/testclasses/JSRRETStressTest", null,
				"java/lang/Object", null);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>",
					"()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "method", "(I)Z", null, null);
			mv.visitCode();

			Label LTryStart1 = new Label();
			Label LTryEnd1 = new Label();
			Label LTryHandle1 = new Label();

			Label LTryStart2 = LTryHandle1;
			Label LTryEnd2 = new Label();
			Label LTryHandle2 = LTryHandle1;

			Label LJSR = new Label();
			Label LReturn = new Label();

			mv.visitTryCatchBlock(LTryStart1, LTryEnd1, LTryHandle1, null);
			mv.visitTryCatchBlock(LTryStart2, LTryEnd2, LTryHandle2, null);

			/* int val = 0 */
			mv.visitInsn(ICONST_0);
			mv.visitVarInsn(ISTORE, 2);
			/* boolean ret = false */
			mv.visitInsn(ICONST_0);
			mv.visitVarInsn(ISTORE, 3);

			/* try block */
			{
				mv.visitLabel(LTryStart1);
				mv.visitVarInsn(ILOAD, 1);

				Label LD = new Label();
				Label C0 = new Label();
				Label C1 = new Label();
				Label C2 = new Label();
				Label C3 = new Label();
				Label C4 = new Label();
				Label C5 = new Label();
				Label C6 = new Label();
				Label C7 = new Label();
				Label C8 = new Label();
				Label C9 = new Label();

				/* switch(val) */
				mv.visitLookupSwitchInsn(LD, new int[] { 0, 1, 2, 3, 4, 5, 6,
						7, 8, 9 }, new Label[] { C0, C1, C2, C3, C4, C5, C6,
						C7, C8, C9 });

				/* case 0 */
				mv.visitLabel(C0);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(0);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 1 */
				mv.visitLabel(C1);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(1);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 2 */
				mv.visitLabel(C2);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(2);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 3 */
				mv.visitLabel(C3);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(3);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 4 */
				mv.visitLabel(C4);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(4);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 5 */
				mv.visitLabel(C5);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(5);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 6 */
				mv.visitLabel(C6);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(6);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 7 */
				mv.visitLabel(C7);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(7);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 8 */
				mv.visitLabel(C8);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(8);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);
				
				/* case 9 */
				mv.visitLabel(C9);
				mv.visitVarInsn(ILOAD, 1);
				mv.visitLdcInsn(9);
				mv.visitInsn(IADD);
				mv.visitVarInsn(ISTORE, 2);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitJumpInsn(GOTO, LReturn);

				/* default */
				mv.visitLabel(LD);
				mv.visitJumpInsn(JSR, LJSR);
			}

			mv.visitLabel(LTryEnd1);
			mv.visitJumpInsn(GOTO, LReturn);

			/* default catch block */
			{
				mv.visitLabel(LTryStart2); /*
											 * label LTryStart2 is same as
											 * LTryHandle1 and LTryHandle2
											 */
				mv.visitVarInsn(ASTORE, 4);
				mv.visitJumpInsn(JSR, LJSR);
				mv.visitLabel(LTryEnd2);
				mv.visitVarInsn(ALOAD, 4);
				mv.visitInsn(ATHROW);
			}

			/* sub-routine block */
			{
				Label LNestedTryStart1 = LJSR;
				Label LNestedTryEnd1 = new Label();
				Label LNestedTryHandle1 = new Label();

				Label LNestedTryStart2 = LNestedTryHandle1;
				Label LNestedTryEnd2 = new Label();
				Label LNestedTryHandle2 = LNestedTryHandle1;

				Label LNestedJSR = new Label();

				mv.visitTryCatchBlock(LNestedTryStart1, LNestedTryEnd1,
						LNestedTryHandle1, null);
				mv.visitTryCatchBlock(LNestedTryStart2, LNestedTryEnd2,
						LNestedTryHandle2, null);

				/* try block */
				{
					mv.visitLabel(LJSR); /*
										 * label LJSR is same as
										 * LNestedTryStart1
										 */
					mv.visitVarInsn(ASTORE, 5);
					mv.visitVarInsn(ILOAD, 2);

					Label LD = new Label();
					Label C0 = new Label();
					Label C1 = new Label();
					Label C2 = new Label();
					Label C3 = new Label();
					Label C4 = new Label();
					Label C5 = new Label();
					Label C6 = new Label();
					Label C7 = new Label();
					Label C8 = new Label();
					Label C9 = new Label();

					Label LJmpToJSR = new Label();

					/* switch(val) */
					mv.visitLookupSwitchInsn(LD, new int[] { 0, 2, 4, 6, 8, 10,
							12, 14, 16, 18, 20 }, new Label[] { C0, C1, C2, C3,
							C4, C5, C6, C7, C8, C9 });

					/* case 0 */
					mv.visitLabel(C0);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(0);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 2 */
					mv.visitLabel(C1);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(1);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 4 */
					mv.visitLabel(C2);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(2);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 6 */
					mv.visitLabel(C3);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(3);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 8 */
					mv.visitLabel(C4);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(4);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 10 */
					mv.visitLabel(C5);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(5);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 12 */
					mv.visitLabel(C6);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(6);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 14 */
					mv.visitLabel(C7);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(7);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 16 */
					mv.visitLabel(C8);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(8);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* case 18 */
					mv.visitLabel(C9);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitLdcInsn(9);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitJumpInsn(GOTO, LNestedTryEnd1);

					/* default */
					mv.visitLabel(LD);
					mv.visitVarInsn(ILOAD, 1);
					mv.visitInsn(ICONST_0);
					mv.visitInsn(ISUB);
					mv.visitVarInsn(ISTORE, 2);

					mv.visitLabel(LJmpToJSR);
					mv.visitJumpInsn(JSR, LNestedJSR);
				}

				mv.visitLabel(LNestedTryEnd1);
				mv.visitVarInsn(RET, 5);

				/* default catch block */
				{
					mv.visitLabel(LNestedTryStart2); /*
													 * label LNestedTryStart2 is
													 * same as LNestedTryHandle1
													 * and LNestedTryHandle2
													 */
					mv.visitVarInsn(ASTORE, 4);
					mv.visitJumpInsn(JSR, LNestedJSR);
					mv.visitLabel(LNestedTryEnd2);
					mv.visitVarInsn(ALOAD, 4);
					mv.visitInsn(ATHROW);
				}

				/* (nested) sub-routine block */
				{
					Label L1 = new Label();
					Label L2 = new Label();

					mv.visitLabel(LNestedJSR);
					mv.visitVarInsn(ASTORE, 6);
					mv.visitVarInsn(ILOAD, 2);
					mv.visitVarInsn(ILOAD, 1);
					mv.visitJumpInsn(IF_ICMPNE, L1);
					mv.visitInsn(ICONST_1);
					mv.visitJumpInsn(GOTO, L2);
					mv.visitLabel(L1);
					mv.visitInsn(ICONST_0);
					mv.visitLabel(L2);
					mv.visitVarInsn(ISTORE, 3);
					mv.visitVarInsn(RET, 6);
				}
			}

			mv.visitLabel(LReturn);
			mv.visitVarInsn(ILOAD, 3);
			mv.visitInsn(IRETURN);

			mv.visitMaxs(2, 7);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}
}
