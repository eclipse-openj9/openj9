/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

import static org.objectweb.asm.Opcodes.*;

import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.reflect.Field;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.List;

public class ValueTypeGenerator extends ClassLoader {
	private static ValueTypeGenerator generator;
	
	/* workaround till the new ASM is released */
	public static final int DEFAULTVALUE = 203;
	public static final int WITHFIELD = 204;
	private static final int ACC_VALUE_TYPE = 0x100;
	
	static {
		generator = new ValueTypeGenerator();
	}
	
	private static byte[] generateClass(String className, String[] fields, boolean isVerifiable, boolean isRef) {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;

		if (isRef) {
			cw.visit(55, ACC_PUBLIC + ACC_FINAL + ACC_SUPER, className, null, "java/lang/Object", null);
		} else {
			cw.visit(55, ACC_PUBLIC + ACC_FINAL + ACC_SUPER + ACC_VALUE_TYPE, className, null, "java/lang/Object", null);
		}

		cw.visitSource(className + ".java", null);
		
		int makeMaxLocal = 0;
		String makeValueSig = "";
		String makeValueGenericSig = "";
		for (String s : fields) {
			String nameAndSigValue[] = s.split(":");
			final int fieldModifiers;
			if (isRef) {
				fieldModifiers = ACC_PUBLIC;
			} else {
				fieldModifiers = ACC_PUBLIC + ACC_FINAL;
			}
			fv = cw.visitField(fieldModifiers, nameAndSigValue[0], nameAndSigValue[1], null, null);
			fv.visitEnd();
			makeValueSig += nameAndSigValue[1];
			makeValueGenericSig += "Ljava/lang/Object;";
			if (nameAndSigValue[1].equals("J") || nameAndSigValue[1].equals("D")) {
				makeMaxLocal += 2;
			} else {
				makeMaxLocal += 1;
			}
			generateFieldMethods(cw, nameAndSigValue, className, isVerifiable, isRef);
		}
		
		initHelper(cw);
		
		if (isRef) {
			makeRef(cw, className, makeValueSig, makeValueGenericSig, fields, makeMaxLocal);
			if (!isVerifiable) {
				makeGeneric(cw, className, "makeRefGeneric", "makeRef", makeValueSig, makeValueGenericSig, fields, makeMaxLocal);
				/* makeValue is invalid on ref: Included to test if runtime error is (correctly) thrown */
				makeValue(cw, className, makeValueSig, fields, makeMaxLocal);
			}

			testWithFieldOnNonValueType(cw, className, fields);
			testWithFieldOnNull(cw, className, fields);
			testWithFieldOnNonExistentClass(cw, className, fields);
			testMonitorEnterOnObject(cw, className, fields);
			testMonitorExitOnObject(cw, className, fields);
			testMonitorEnterAndExitWithRefType(cw, className, fields);
		} else {
			makeValue(cw, className, makeValueSig, fields, makeMaxLocal);
			if (!isVerifiable) {
				makeGeneric(cw, className, "makeValueGeneric", "makeValue", makeValueSig, makeValueGenericSig, fields, makeMaxLocal);
			}
		}

		cw.visitEnd();
		return cw.toByteArray();
		
	}

	private static void generateFieldMethods(ClassWriter cw, String[] nameAndSigValue, String className, boolean isVerifiable, boolean isRef) {
		generateGetter(cw, nameAndSigValue, className);
		generateSetter(cw, nameAndSigValue, className);
		generateWither(cw, nameAndSigValue, className);
		if (!isVerifiable) {
			generateGetterGeneric(cw, nameAndSigValue, className);
			generateWitherGeneric(cw, nameAndSigValue, className);
			generateSetterGeneric(cw, nameAndSigValue, className);
		}
	}
	
	private static void initHelper(ClassWriter cw) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		mv.visitInsn(RETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
	}
	
	/*
	 * This function should only be called in the 
	 * TestWithFieldOnNonValueType test
	 */
	private static void testWithFieldOnNonValueType(ClassWriter cw, String className, String[] fields) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testWithFieldOnNonValueType", "()Ljava/lang/Object;", null, null);
		mv.visitCode();
		mv.visitTypeInsn(NEW, className);
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, className, "<init>", "()V", false);
		mv.visitLdcInsn(new Long(123L));
		mv.visitFieldInsn(WITHFIELD, className, "longField", "J");
		mv.visitInsn(ARETURN);
		mv.visitMaxs(1, 2);
		mv.visitEnd();
	}
	
	/*
	 * This function should only be called in the 
	 * TestWithFieldOnNull test
	 */
	private static void testWithFieldOnNull(ClassWriter cw, String className, String[] fields) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testWithFieldOnNull", "()Ljava/lang/Object;", null, null);
		mv.visitCode();
		mv.visitInsn(ACONST_NULL);
		mv.visitLdcInsn(new Long(123L));
		mv.visitFieldInsn(WITHFIELD, className, "longField", "J");
		mv.visitInsn(ARETURN);
		mv.visitMaxs(1, 2);
		mv.visitEnd();
	}
	
	/*
	 * This function should only be called in the 
	 * TestWithFieldOnNonExistentClass test
	 */
	private static void testWithFieldOnNonExistentClass(ClassWriter cw, String className, String[] fields) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testWithFieldOnNonExistentClass", "()Ljava/lang/Object;", null, null);
		mv.visitCode();
		mv.visitTypeInsn(NEW, className);
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, className, "<init>", "()V", false);
		mv.visitLdcInsn(new Long(123L));
		mv.visitFieldInsn(WITHFIELD, "NonExistentClass", "longField", "J");
		mv.visitInsn(ARETURN);
		mv.visitMaxs(1, 2);
		mv.visitEnd();
	}

	 /* 
	  * This function should only be called in the
	  * TestMonitorEnterOnValueType test and 
	  * TestMonitorEnterWithRefType test
	  */
	private static void testMonitorEnterOnObject(ClassWriter cw, String className, String[] fields) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testMonitorEnterOnObject", "(Ljava/lang/Object;)V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(MONITORENTER);
		mv.visitInsn(RETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
	}

	 /* 
	  * This function should only be called in the
	  * TestMonitorExitOnValueType test and
	  * TestMonitorExitWithRefType test
	  */
	private static void testMonitorExitOnObject(ClassWriter cw, String className, String[] fields) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testMonitorExitOnObject", "(Ljava/lang/Object;)V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(MONITOREXIT);
		mv.visitInsn(RETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
	}

	 /* 
	  * This function should only be called in the
	  * TestMonitorEnterAndExitWithRefType test
	  */
	private static void testMonitorEnterAndExitWithRefType(ClassWriter cw, String className, String[] fields) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testMonitorEnterAndExitWithRefType", "(Ljava/lang/Object;)V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(DUP);
		mv.visitInsn(MONITORENTER);
		mv.visitInsn(MONITOREXIT);
		mv.visitInsn(RETURN);
		mv.visitMaxs(2,1);
		mv.visitEnd();
	}

	private static void makeValue(ClassWriter cw, String valueName, String makeValueSig, String[] fields, int makeMaxLocal) {
		boolean doubleDetected = false;
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC  + ACC_STATIC, "makeValue", "(" + makeValueSig + ")L" + valueName + ";", null, null);
		mv.visitCode();
		mv.visitTypeInsn(DEFAULTVALUE, valueName);
		for (int i = 0, count = 0; i <  fields.length; i++) {
			String nameAndSig[] = fields[i].split(":");
			switch (nameAndSig[1]) {
			case "D":
				mv.visitVarInsn(DLOAD, count);
				doubleDetected = true;
				count += 2;
				break;
			case "I":
			case "Z":
			case "B":
			case "C":
			case "S":
				mv.visitVarInsn(ILOAD, count);
				count++;
				break;
			case "F":
				mv.visitVarInsn(FLOAD, count);
				count++;
				break;
			case "J":
				mv.visitVarInsn(LLOAD, count);
				doubleDetected = true;
				count += 2;
				break;
			default:
				mv.visitVarInsn(ALOAD, count);
				count++;
				break;
			}
			mv.visitFieldInsn(WITHFIELD, valueName, nameAndSig[0], nameAndSig[1]);
		}
		mv.visitInsn(ARETURN);
		int maxStack = (doubleDetected) ? 3 : 2;
		mv.visitMaxs(maxStack, makeMaxLocal);
		mv.visitEnd();
	}
	
	private static void makeGeneric(ClassWriter cw, String className, String methodName, String specificMethodName, String makeValueSig, String makeValueGenericSig, String[] fields, int makeMaxLocal) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC  + ACC_STATIC, methodName, "(" + makeValueGenericSig + ")L" + className + ";", null, new String[] {"java/lang/Exception"});
		mv.visitCode();
		for (int i = 0; i <  fields.length; i++) {
			mv.visitVarInsn(ALOAD, i);
			String nameAndSigValue[] = fields[i].split(":");
			switch (nameAndSigValue[1]) {
			case "D":
				mv.visitTypeInsn(CHECKCAST, "java/lang/Double");
				mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Double", "doubleValue", "()D", false);
				break;
			case "I":
				mv.visitTypeInsn(CHECKCAST, "java/lang/Integer");
				mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Integer", "intValue", "()I", false);
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
		}
		mv.visitMethodInsn(INVOKESTATIC, className, specificMethodName, "(" + makeValueSig + ")L"+ className + ";", false);
		mv.visitInsn(ARETURN);

		mv.visitMaxs(makeMaxLocal, makeMaxLocal);
		mv.visitEnd();
	}
	
	private static void makeRef(ClassWriter cw, String className, String makeValueSig, String makeValueGenericSig, String[]fields, int makeMaxLocal) {
		boolean doubleDetected = false;
		int makeRefArgsAndLocals = makeMaxLocal + 1; //extra slot is to store the ref being created
		
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC  + ACC_STATIC, "makeRef", "(" + makeValueSig + ")L" + className + ";", null, null);
		mv.visitCode();
		mv.visitTypeInsn(NEW, className);
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, className, "<init>", "()V", false);
		mv.visitVarInsn(ASTORE, makeMaxLocal);
		for (int i = 0, count = 0; i < fields.length; i++) {
			mv.visitVarInsn(ALOAD, makeMaxLocal);
			String nameAndSigValue[] = fields[i].split(":");
			switch (nameAndSigValue[1]) {
			case "D":
				mv.visitVarInsn(DLOAD, count);
				doubleDetected = true;
				count += 2;
				break;
			case "I":
			case "Z":
			case "B":
			case "C":
			case "S":
				mv.visitVarInsn(ILOAD, count);
				count++;
				break;
			case "F":
				mv.visitVarInsn(FLOAD, count);
				count++;
				break;
			case "J":
				mv.visitVarInsn(LLOAD, count);
				doubleDetected = true;
				count += 2;
				break;
			default:
				mv.visitVarInsn(ALOAD, count);
				count++;
				break;
			}
			mv.visitFieldInsn(PUTFIELD, className, nameAndSigValue[0], nameAndSigValue[1]);
		}
		mv.visitVarInsn(ALOAD, makeMaxLocal);
		mv.visitInsn(ARETURN);
		
		int maxStack = (doubleDetected) ? 3 : 2;
		mv.visitMaxs(maxStack, makeRefArgsAndLocals);
		mv.visitEnd();
	}

	private static void generateSetter(ClassWriter cw, String[] nameAndSigValue, String className) {
		boolean doubleDetected = false;
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "set" + nameAndSigValue[0], "(" + nameAndSigValue[1] + ")V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitVarInsn(DLOAD, 1);
			doubleDetected = true;
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
			doubleDetected = true;
			break;
		default:
			mv.visitVarInsn(ALOAD, 1);
			break;
		}
		mv.visitFieldInsn(PUTFIELD, className, nameAndSigValue[0], nameAndSigValue[1]);
		mv.visitInsn(RETURN);
		int maxStackAndLocals = (doubleDetected ? 3 : 2);
		mv.visitMaxs(maxStackAndLocals, maxStackAndLocals);
		mv.visitEnd();
	}
	
	private static void generateSetterGeneric(ClassWriter cw, String[] nameAndSigValue, String className) {
		boolean doubleDetected = false;
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "setGeneric" + nameAndSigValue[0], "(Ljava/lang/Object;)V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitVarInsn(ALOAD, 1);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Double");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Double", "doubleValue", "()D", false);
			doubleDetected = true;
			break;
		case "I":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Integer");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Integer", "intValue", "()I", false);
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
			mv.visitTypeInsn(CHECKCAST, "java/lang/Long");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Long", "longValue", "()J", false);
			doubleDetected = true;
			break;
		default:
			break;
		}
		mv.visitMethodInsn(INVOKEVIRTUAL, className, "set" + nameAndSigValue[0], "(" + nameAndSigValue[1] + ")V", false);
		mv.visitInsn(RETURN);
		int maxStack = (doubleDetected ? 3 : 2);
		mv.visitMaxs(maxStack, 2);
		mv.visitEnd();
	}

	private static void generateWither(ClassWriter cw, String[] nameAndSigValue, String className) {
		boolean doubleDetected = false;
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "with" + nameAndSigValue[0], "(" + nameAndSigValue[1] + ")L" + className + ";", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitVarInsn(DLOAD, 1);
			doubleDetected = true;
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
			doubleDetected = true;
			break;
		default:
			mv.visitVarInsn(ALOAD, 1);
			break;
		}
		mv.visitFieldInsn(WITHFIELD, className, nameAndSigValue[0], nameAndSigValue[1]);
		mv.visitInsn(ARETURN);
		int maxStackAndLocals = (doubleDetected ? 3 : 2);
		mv.visitMaxs(maxStackAndLocals, maxStackAndLocals);
		mv.visitEnd();
	}
		
	private static void generateWitherGeneric(ClassWriter cw, String[] nameAndSigValue, String className) {
		boolean doubleDetected = false;
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "withGeneric" + nameAndSigValue[0], "(Ljava/lang/Object;)L" + className + ";", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitVarInsn(ALOAD, 1);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Double");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Double", "doubleValue", "()D", false);
			doubleDetected = true;
			break;
		case "I":
			mv.visitTypeInsn(CHECKCAST, "java/lang/Integer");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Integer", "intValue", "()I", false);
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
			mv.visitTypeInsn(CHECKCAST, "java/lang/Long");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Long", "longValue", "()J", false);
			doubleDetected = true;
			break;
		default:
			break;
		}

		mv.visitMethodInsn(INVOKEVIRTUAL, className, "with" + nameAndSigValue[0], "(" + nameAndSigValue[1] + ")L" + className + ";", false);
		mv.visitInsn(ARETURN);
		int maxStack = (doubleDetected ? 3 : 2);
		mv.visitMaxs(maxStack, 2);
		mv.visitEnd();
	}
	
	private static void generateGetter(ClassWriter cw, String[] nameAndSigValue, String className) {
		boolean doubleDetected = false;
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "get" + nameAndSigValue[0], "()" + nameAndSigValue[1], null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitFieldInsn(GETFIELD, className, nameAndSigValue[0], nameAndSigValue[1]);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitInsn(DRETURN);
			doubleDetected = true;
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
			doubleDetected = true;
			break;
		default:
			mv.visitInsn(ARETURN);
			break;
		}
		int maxStack = (doubleDetected ? 2 : 1);
		mv.visitMaxs(maxStack, 1);
		mv.visitEnd();
	}
	
	private static void generateGetterGeneric(ClassWriter cw, String[] nameAndSigValue, String className) {
		boolean doubleDetected = false;
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "getGeneric" + nameAndSigValue[0], "()Ljava/lang/Object;", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKEVIRTUAL, className, "get" + nameAndSigValue[0], "()" + nameAndSigValue[1], false);
		switch (nameAndSigValue[1]) {
		case "D":
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/Double", "valueOf", "(D)Ljava/lang/Double;", false);
			doubleDetected = true;
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
			doubleDetected = true;
			break;
		default:
			break;
		}

		mv.visitInsn(ARETURN);
		int maxStack = (doubleDetected ? 2 : 1);
		mv.visitMaxs(maxStack, 1);
		mv.visitEnd();
	}

	public static Class<?> generateValueClass(String name, String[] fields) throws Throwable {
		byte[] bytes = generateClass(name, fields, false, false);
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateVerifiableValueClass(String name, String[] fields) throws Throwable {
		byte[] bytes = generateClass(name, fields, true, false);
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateRefClass(String name, String[] fields) throws Throwable {
		byte[] bytes = generateClass(name, fields, false, true);
		return generator.defineClass(name, bytes, 0, bytes.length);
	}
}