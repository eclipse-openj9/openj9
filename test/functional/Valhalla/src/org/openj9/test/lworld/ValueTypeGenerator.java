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
package org.openj9.test.lworld;

import org.objectweb.asm.*;

import jdk.internal.misc.Unsafe;

import static org.objectweb.asm.Opcodes.*;

import java.lang.reflect.Field;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.List;

public class ValueTypeGenerator {
	private static Unsafe unsafe;
	
	/* workaround till the new ASM is released */
	public static final int DEFAULTVALUE = 187; //TODO update this when bytecode support is added
	public static final int WITHFIELD = 181; //TODO update this when bytecode support is added
	private static final int ACC_VALUE_TYPE = 0x100;
	
	static {
		try {
			Field f = Unsafe.class.getDeclaredField("theUnsafe");
			f.setAccessible(true);
			unsafe = (Unsafe) f.get(null);
		} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
			e.printStackTrace();
		}	
	}
	
	private static byte[] generateValue(String valueName, String[] fields) {
		ArrayList<String> valueTypesList = new ArrayList<String>();
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;

		cw.visit(V1_8, ACC_PUBLIC + ACC_FINAL + ACC_SUPER + ACC_VALUE_TYPE, valueName, null, "java/lang/Object", null);

		cw.visitSource(valueName + ".java", null);
		
		valueTypesList.add(valueName);
		
		//TODO this will be removed once bytecode support is added
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		

		String makeValueSig = "";
		String makeValueGenericSig = "";
		for (String s : fields) {
			String nameAndSigValue[] = s.split(":");
			int fieldModifiers = ACC_PUBLIC + ACC_FINAL;
			if (nameAndSigValue.length > 2 && nameAndSigValue[2].equals("value")) {
				valueTypesList.add(nameAndSigValue[1].substring(1, nameAndSigValue[1].length() - 1));
			}
			fv = cw.visitField(fieldModifiers, nameAndSigValue[0], nameAndSigValue[1], null, null);
			fv.visitEnd();
			makeValueSig += nameAndSigValue[1];
			makeValueGenericSig += "Ljava/lang/Object;";
			
			generateGetter(cw, nameAndSigValue, valueName);
			
			generateSetter(cw, nameAndSigValue, valueName);
			
			generateWither(cw, nameAndSigValue, valueName);
			
		}
		
		cw.visitEnd();
		
		{
			boolean doubleDetected = false;
			mv = cw.visitMethod(ACC_PUBLIC  + ACC_STATIC, "makeValue", "(" + makeValueSig + ")L" + valueName + ";", null, null);
			mv.visitCode();
			mv.visitTypeInsn(NEW, valueName); //TODO new+dup+init will be removed once bytecode support for valuetypes is added
			mv.visitInsn(DUP);
			mv.visitMethodInsn(INVOKESPECIAL, valueName, "<init>", "()V", false);
			mv.visitVarInsn(ASTORE, fields.length);
			for (int i = 0; i <  fields.length; i++) {
				String nameAndSig[] = fields[i].split(":");
				mv.visitVarInsn(ALOAD, fields.length);
				switch (nameAndSig[1]) {
				case "D":
					doubleDetected = true;
					mv.visitVarInsn(DLOAD, i);
					break;
				case "I":
				case "Z":
				case "B":
				case "C":
				case "S":
					mv.visitVarInsn(ILOAD, i);
					break;
				case "F":
					mv.visitVarInsn(FLOAD, i);
					break;
				case "J":
					doubleDetected = true;
					mv.visitVarInsn(LLOAD, i);
					break;
				default:
					mv.visitVarInsn(ALOAD, i);
					break;
				}
				mv.visitFieldInsn(WITHFIELD, valueName, nameAndSig[0], nameAndSig[1]);
			}
			mv.visitVarInsn(ALOAD, fields.length);
			mv.visitInsn(ARETURN);
			
			int maxStackAndLocals = (doubleDetected) ? 3 : 2;
			mv.visitMaxs(maxStackAndLocals, maxStackAndLocals + 1);
			mv.visitEnd();
		}

		cw.visitEnd();

		return cw.toByteArray();
	}
	
	public static byte[] generateRefObject(String className, String[] fields) {
		ArrayList<String> valueTypesList = new ArrayList<String>();
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;

		cw.visit(V1_8, ACC_PUBLIC + ACC_FINAL + ACC_SUPER, className, null, "java/lang/Object", null);

		cw.visitSource("Value.java", null);

		String makeValueGenericSig = "";
		String makeValueSig = "";
		int makeMaxLocal = 0;
		for (String s : fields) {
			String nameAndSigValue[] = s.split(":");
			int fieldModifiers = ACC_PUBLIC;
			if (nameAndSigValue.length > 2 && nameAndSigValue[2].equals("value")) {
				valueTypesList.add(nameAndSigValue[1].substring(1, nameAndSigValue[1].length() - 1));
			}
			fv = cw.visitField(fieldModifiers, nameAndSigValue[0], nameAndSigValue[1], null, null);
			fv.visitEnd();
			makeValueGenericSig += "Ljava/lang/Object;";
			makeValueSig += nameAndSigValue[1];
			if (nameAndSigValue[1].equals("J") || nameAndSigValue[1].equals("D")) {
				makeMaxLocal += 2;
			} else {
				makeMaxLocal += 1;
			}
			
			
			generateGetter(cw, nameAndSigValue, className);
			
			generateSetter(cw, nameAndSigValue, className);
		}
		
		cw.visitEnd();
		
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		
		{
			int maxStack = 0;
			mv = cw.visitMethod(ACC_PUBLIC  + ACC_STATIC, "makeRefGeneric", "(" + makeValueGenericSig + ")L" + className + ";", null, new String[] {"java/lang/Exception"});
			mv.visitCode();
			for (int i = 0; i <  fields.length; i++) {
				mv.visitVarInsn(ALOAD, i);
				String nameAndSigValue[] = fields[i].split(":");
				switch (nameAndSigValue[1]) {
				case "D":
					maxStack += 2;
					mv.visitTypeInsn(CHECKCAST, "java/lang/Double");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Double", "doubleValue", "()D", false);
					break;
				case "I":
					maxStack++;
					mv.visitTypeInsn(CHECKCAST, "java/lang/Integer");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Integer", "intValue", "()I", false);
				case "Z":
					maxStack++;
					mv.visitTypeInsn(CHECKCAST, "java/lang/Boolean");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Boolean", "booleanValue", "()Z", false);
				case "B":
					maxStack++;
					mv.visitTypeInsn(CHECKCAST, "java/lang/Byte");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Byte", "byteValue", "()B", false);
				case "C":
					maxStack++;
					mv.visitTypeInsn(CHECKCAST, "java/lang/Character");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Character", "charValue", "()C", false);
				case "S":
					maxStack++;
					mv.visitTypeInsn(CHECKCAST, "java/lang/Short");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Short", "shortValue", "()S", false);
					break;
				case "F":
					maxStack++;
					mv.visitTypeInsn(CHECKCAST, "java/lang/Float");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Float", "floatValue", "()F", false);
					break;
				case "J":
					maxStack += 2;
					mv.visitTypeInsn(CHECKCAST, "java/lang/Double");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Double", "doubleValue", "()J", false);
					break;
				default:
					maxStack++;
					break;
				}
			}
			
			mv.visitMethodInsn(INVOKESTATIC, className, "makeRef", "(" + makeValueSig + ")L"+ className + ";", false);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(maxStack, makeMaxLocal + 1);
			mv.visitEnd();
		}
		
		{
			boolean doubleDetected = false;
			
			mv = cw.visitMethod(ACC_PUBLIC  + ACC_STATIC, "makeRef", "(" + makeValueSig + ")L" + className + ";", null, null);
			mv.visitCode();
			mv.visitTypeInsn(NEW, className);
			mv.visitInsn(DUP);
			mv.visitMethodInsn(INVOKESPECIAL, className, "<init>", "()V", false);
			mv.visitVarInsn(ASTORE, fields.length);
			for (int i = 0; i <  fields.length; i++) {
				mv.visitVarInsn(ALOAD, fields.length);
				String nameAndSigValue[] = fields[i].split(":");
				switch (nameAndSigValue[1]) {
				case "D":
					doubleDetected = true;
					mv.visitVarInsn(DLOAD, i);
					break;
				case "I":
				case "Z":
				case "B":
				case "C":
				case "S":
					mv.visitVarInsn(ILOAD, i);
					break;
				case "F":
					mv.visitVarInsn(FLOAD, i);
					break;
				case "J":
					doubleDetected = true;
					mv.visitVarInsn(LLOAD, i);
					break;
				default:
					mv.visitVarInsn(ALOAD, i);
					break;
				}
				mv.visitFieldInsn(PUTFIELD, className, nameAndSigValue[0], nameAndSigValue[1]);
			}
			mv.visitVarInsn(ALOAD, fields.length);
			mv.visitInsn(ARETURN);
			
			int maxStack = (doubleDetected) ? 3 : 2;
			mv.visitMaxs(maxStack, makeMaxLocal + 1);
			mv.visitEnd();
		}

		cw.visitEnd();

		return cw.toByteArray();
	}

	private static void generateSetter(ClassWriter cw, String[] nameAndSigValue, String className) {
		/* generate setter */
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "set" + nameAndSigValue[0], "(" + nameAndSigValue[1] + ")V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitVarInsn(DLOAD, 1);
			break;
		case "I":
		case "Z":
		case "B":
		case "C":
		case "S":
			mv.visitVarInsn(ILOAD, 1);
			break;
		case "F":
			mv.visitVarInsn(FLOAD, 1);
			break;
		case "J":
			mv.visitVarInsn(LLOAD, 1);
			break;
		default:
			mv.visitVarInsn(ALOAD, 1);
			break;
		}
		mv.visitFieldInsn(PUTFIELD, className, nameAndSigValue[0], nameAndSigValue[1]);
		mv.visitInsn(RETURN);
		mv.visitMaxs(2, 2);
		mv.visitEnd();
		
		mv = cw.visitMethod(ACC_PUBLIC, "setGeneric" + nameAndSigValue[0], "(Ljava/lang/Object;)V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitVarInsn(ALOAD, 1);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Double");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Double", "doubleValue", "()D", false);
			break;
		case "I":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Interger");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Interger", "intValue", "()I", false);
		case "Z":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Boolean");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Boolean", "booleanValue", "()Z", false);
		case "B":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Byte");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Byte", "byteValue", "()B", false);
		case "C":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Character");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Character", "charValue", "()C", false);
		case "S":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Short");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Short", "shortValue", "()S", false);
			break;
		case "F":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Float");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Float", "floatValue", "()F", false);
			break;
		case "J":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Double");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Double", "doubleValue", "()J", false);
			break;
		default:
			break;
		}
		
		mv.visitMethodInsn(INVOKEVIRTUAL, className, "set" + nameAndSigValue[0], "(" + nameAndSigValue[1] + ")V", false);
		mv.visitInsn(RETURN);
		mv.visitMaxs(2, 2);
		mv.visitEnd();
	}
	
	private static void generateWither(ClassWriter cw, String[] nameAndSigValue, String className) {
		/* generate setter */
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "set" + nameAndSigValue[0], "(" + nameAndSigValue[1] + ")L" + className + ";", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitVarInsn(DLOAD, 1);
			break;
		case "I":
		case "Z":
		case "B":
		case "C":
		case "S":
			mv.visitVarInsn(ILOAD, 1);
			break;
		case "F":
			mv.visitVarInsn(FLOAD, 1);
			break;
		case "J":
			mv.visitVarInsn(LLOAD, 1);
			break;
		default:
			mv.visitVarInsn(ALOAD, 1);
			break;
		}
		mv.visitFieldInsn(WITHFIELD, className, nameAndSigValue[0], nameAndSigValue[1]);
		mv.visitInsn(RETURN);
		mv.visitMaxs(2, 2);
		mv.visitEnd();
	}
	
	private static void generateGetter(ClassWriter cw, String[] nameAndSigValue, String className) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "get" + nameAndSigValue[0], "()" + nameAndSigValue[1], null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitFieldInsn(GETFIELD, className, nameAndSigValue[0], nameAndSigValue[1]);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitInsn(DRETURN);
			break;
		case "I":
		case "Z":
		case "B":
		case "C":
		case "S":
			mv.visitInsn(IRETURN);
			break;
		case "F":
			mv.visitInsn(FRETURN);
			break;
		case "J":
			mv.visitInsn(LRETURN);
			break;
		default:
			mv.visitInsn(ARETURN);
			break;
		}
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		
		mv = cw.visitMethod(ACC_PUBLIC, "getGeneric" + nameAndSigValue[0], "()Ljava/lang/Object;", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKEVIRTUAL, className, "get" + nameAndSigValue[0], "()" + nameAndSigValue[1], false);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Double", "valueOf", "(D)Ljava/lang/Double;", false);
			break;
		case "I":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Integer", "valueOf", "(I)Ljava/lang/Integer;", false);
			break;
		case "Z":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Boolean", "valueOf", "(Z)Ljava/lang/Boolean;", false);
			break;
		case "B":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Byte", "valueOf", "(B)Ljava/lang/Byte;", false);
			break;
		case "C":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Character", "valueOf", "(C)Ljava/lang/Character;", false);
			break;
		case "S":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Short", "valueOf", "(S)Ljava/lang/Short;", false);
			break;
		case "F":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Float", "valueOf", "(F)Ljava/lang/Float;", false);
			break;
		case "J":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Long", "valueOf", "(J)Ljava/lang/Long;", false);
			break;
		default:
			break;
		}
		mv.visitInsn(ARETURN);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
	}
	
	private static Class<?> defineClass(String className, byte[] bytes, ClassLoader loader, ProtectionDomain pD) throws Throwable {
		return unsafe.defineClass(className, bytes, 0, bytes.length, loader, pD);
	}
	
	public static Class<?> generateValueClass(String name, String[] fields) throws Throwable {
		return ValueTypeGenerator.defineClass(name, ValueTypeGenerator.generateValue(name, fields), ValueTypeGenerator.class.getClassLoader(), ValueTypeGenerator.class.getProtectionDomain());
	}
	
	public static Class<?> generateRefClass(String name, String[] fields) throws Throwable {
		return ValueTypeGenerator.defineClass(name, ValueTypeGenerator.generateRefObject(name, fields), ValueTypeGenerator.class.getClassLoader(), ValueTypeGenerator.class.getProtectionDomain());
	}
	
}