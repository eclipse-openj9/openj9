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

import static org.objectweb.asm.Opcodes.ACC_PRIVATE;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_STATIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.ASTORE;
import static org.objectweb.asm.Opcodes.GETSTATIC;
import static org.objectweb.asm.Opcodes.IFEQ;
import static org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static org.objectweb.asm.Opcodes.INVOKESTATIC;
import static org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.LCMP;
import static org.objectweb.asm.Opcodes.LCONST_0;
import static org.objectweb.asm.Opcodes.LLOAD;
import static org.objectweb.asm.Opcodes.LREM;
import static org.objectweb.asm.Opcodes.LSTORE;
import static org.objectweb.asm.Opcodes.RETURN;
import static org.objectweb.asm.Opcodes.V1_7;

import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;

public class IntLongObjectAlignmentTestGenerator {

	private static String className;
	private static String fileName;
	private static String parent = "level_2_I";

	public static void main(String[] args) throws Throwable {

		parent = "level_2_I";
		permuteString("", "ijo");
		
		parent = "level_2_J";
		permuteString("", "ijo");
		
		parent = "level_2_O";
		permuteString("", "ijo");
	
	}
	
	public static void printCommand() {
		//System.out.println("java -Xbootclasspath/p:. " + className );
		//System.out.println("java -Xbootclasspath/p:. -Xlockword:noLockword=" + className + " " + className);
		
		System.out.println("<test id=\"Check " + className + " is aligned\">");
		System.out.println("  <command>$EXE$ $TESTSBOOTCLASSPATH$ $OBJECTTESTSBOOTCLASSPATH$ main " + className + "</command>");
		System.out.println("  <return value=\"0\" />");
		System.out.println("  <output type=\"failure\">not aligned</output>");
		System.out.println("  <output type=\"failure\">Unhandled Exception</output>");
		System.out.println("  <output type=\"failure\">Exception:</output>");
		System.out.println("  <output type=\"failure\">Processing dump event</output>");
		System.out.println("</test>");	
		
		System.out.println("<test id=\"Check " + className + " is still aligned with -Xlockword:noLockword\">");
		System.out.println("  <command>$EXE$ $TESTSBOOTCLASSPATH$ $OBJECTTESTSBOOTCLASSPATH$ -Xlockword:noLockword=" + className + " main " + className + "</command>");
		System.out.println("  <return value=\"0\" />");
		System.out.println("  <output type=\"failure\">not aligned</output>");
		System.out.println("  <output type=\"failure\">Unhandled Exception</output>");
		System.out.println("  <output type=\"failure\">Exception:</output>");
		System.out.println("  <output type=\"failure\">Processing dump event</output>");
		System.out.println("</test>");		
	}

	public static void permuteString(String beginningString, String endingString) throws Throwable {
		if (endingString.length() <= 1) {
			
			className = beginningString + endingString + "_extends_" + parent;
			fileName = className + ".class";
		
			FileOutputStream fos = new FileOutputStream(fileName);
			fos.write(makeExample("level_2_I", beginningString + endingString));
			fos.flush();
			fos.close();
			printCommand();
		} else {
			for (int i = 0; i < endingString.length(); i++) {
				String newString = endingString.substring(0, i) + endingString.substring(i + 1);
				permuteString(beginningString + endingString.charAt(i), newString);
			}
		}
	}

	public static byte[] makeExample(String parent, String args)
			throws Throwable {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv = null;
		MethodVisitor mv;
		cw.visit(V1_7, ACC_SUPER, className, null, parent, null);

		char argsArr[] = args.toCharArray();
		for (char ch : argsArr) {
			switch (ch) {
			case 'i':
				fv = cw.visitField(ACC_PRIVATE, "i", "I", null, null);
				break;
			case 'j':
				fv = cw.visitField(ACC_PRIVATE, "l", "J", null, null);
				break;
			case 'o':
				fv = cw.visitField(ACC_PRIVATE, "o", "Ljava/lang/Object;",
						null, null);
				break;
			default:
			}
			fv.visitEnd();
		}

		{
			mv = cw.visitMethod(0, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, parent, "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		
		cw.visitEnd();

		return cw.toByteArray();

	}
}
