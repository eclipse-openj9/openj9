/*
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.lworld;

import org.objectweb.asm.*;

import static org.objectweb.asm.Opcodes.*;

public class StrictFieldGenerator extends ClassLoader {
    private static StrictFieldGenerator generator = new StrictFieldGenerator();

    public static Class<?> generateTestPutStrictFinalFieldLateLarval() {
        String className = "TestPutStrictFinalFieldLateLarval";
        return generateTestPutStrictFinalField(className, true, false);
    }

    public static Class<?> generateTestPutStrictFinalFieldUnrestricted() {
        String className = "TestPutStrictFinalFieldUnrestricted";
        return generateTestPutStrictFinalField(className, false, true);
    }

    private static Class<?> generateTestPutStrictFinalField(String className, boolean lateLarval, boolean unrestricted) {
        String fieldName = "i";
        String fieldDesc = "I";

        ClassWriter cw = new ClassWriter(0);
        cw.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION,
            ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY,
            className, null, "java/lang/Object", null);

        cw.visitField(ValhallaUtils.ACC_STRICT_INIT | ACC_FINAL, fieldName, fieldDesc, null, null);

        MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
        mv.visitCode();
        mv.visitVarInsn(ALOAD, 0);
        mv.visitInsn(ICONST_0);
        mv.visitFieldInsn(PUTFIELD, className, fieldName, fieldDesc);
        mv.visitVarInsn(ALOAD, 0);
        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
        if (lateLarval) {
            mv.visitVarInsn(ALOAD, 0);
            mv.visitInsn(ICONST_0);
            mv.visitFieldInsn(PUTFIELD, className, fieldName, fieldDesc);
        }
        mv.visitInsn(RETURN);
        mv.visitMaxs(2, 1);
        mv.visitEnd();

        if (unrestricted) {
            mv = cw.visitMethod(ACC_PUBLIC, "putI", "(I)V", null, null);
            mv.visitCode();
            mv.visitVarInsn(ALOAD, 0);
            mv.visitVarInsn(ILOAD, 1);
            mv.visitFieldInsn(PUTFIELD, className, fieldName, fieldDesc);
            mv.visitInsn(RETURN);
            mv.visitMaxs(2, 2);
            mv.visitEnd();
        }

        cw.visitEnd();
        byte[] bytes = cw.toByteArray();
        return generator.defineClass(className, cw.toByteArray(), 0, bytes.length);
    }

    public static Class<?> generateTestGetStaticInLarvalForUnsetStrictField() {
        String className = "TestGetStaticInLarvalForUnsetStrictField";
        return generateTestPutGetStrictStaticField(className, false, false, true, false);
    }

    public static Class<?> generateTestPutStaticInLarvalForReadStrictFinalField() {
        String className = "TestPutStaticInLarvalForReadStrictFinalField";
        return generateTestPutGetStrictStaticField(className, false, true, true, true);
    }

    public static Class<?> generateTestStrictStaticFieldNotSetInLarval() {
        String className = "TestStrictStaticFieldNotSetInLarval";
        return generateTestPutGetStrictStaticField(className, false, false, false, false);
    }

    public static Class<?> generateTestStrictStaticFieldNotSetInLarvalMulti() {
        String className = "TestStrictStaticFieldNotSetInLarvalMulti";
        return generateTestPutGetStrictStaticField(className, true, true, false, false);
    }

    public static Class<?> generateTestStrictStaticFieldPutTwice() {
        String className = "TestStrictStaticFieldPutTwice";
        return generateTestPutGetStrictStaticField(className, false, true, false, true);
    }

    private static Class<?> generateTestPutGetStrictStaticField(String className, boolean secondField, boolean put1, boolean get, boolean put2) {
        String fieldName = "i";
        String fieldDesc = "I";
        String fieldName2 = "i2";

        ClassWriter cw = new ClassWriter(0);
        cw.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION,
            ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY,
            className, null, "java/lang/Object", null);

        cw.visitField(ACC_STATIC | ValhallaUtils.ACC_STRICT_INIT | ACC_FINAL, fieldName, fieldDesc, null, null);
        if (secondField) {
            cw.visitField(ACC_STATIC | ValhallaUtils.ACC_STRICT_INIT | ACC_FINAL, fieldName2, fieldDesc, null, null);
        }

        MethodVisitor mv = cw.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
        mv.visitCode();
        if (put1) {
            mv.visitInsn(ICONST_1);
            mv.visitFieldInsn(PUTSTATIC, className, fieldName, fieldDesc);
        }
        if (get) {
            mv.visitFieldInsn(GETSTATIC, className, fieldName, fieldDesc);
        }
        if (put2) {
            mv.visitInsn(ICONST_2);
            mv.visitFieldInsn(PUTSTATIC, className, fieldName, fieldDesc);
        }
        mv.visitInsn(RETURN);
        mv.visitMaxs(2, 0);
        mv.visitEnd();

        mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
        mv.visitCode();
        mv.visitVarInsn(ALOAD, 0);
        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
        mv.visitInsn(RETURN);
        mv.visitMaxs(1, 1);
        mv.visitEnd();

        cw.visitEnd();
        byte[] bytes = cw.toByteArray();

        return generator.defineClass(className, cw.toByteArray(), 0, bytes.length);
    }

    static Class<?> generateTestStrictStaticFieldObject() {
        String className = "TestGenerateTestStrictStaticFieldObject";
        String fieldName = "o";
        String fieldType = "java/lang/Object";
        String fieldDesc = "L" + fieldType + ";";

        ClassWriter cw = new ClassWriter(0);
        cw.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION,
            ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY,
            className, null, "java/lang/Object", null);
        cw.visitField(ACC_STATIC | ValhallaUtils.ACC_STRICT_INIT, fieldName, fieldDesc, null, null);
        MethodVisitor mv = cw.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
        mv.visitCode();
        /* putstatic */
        mv.visitTypeInsn(NEW, fieldType);
        mv.visitInsn(DUP);
        mv.visitMethodInsn(INVOKESPECIAL, fieldType, "<init>", "()V", false);
        mv.visitFieldInsn(PUTSTATIC, className, fieldName, fieldDesc);
        /* getstatic */
        mv.visitFieldInsn(GETSTATIC, className, fieldName, fieldDesc);
        mv.visitInsn(RETURN);
        mv.visitMaxs(2, 1);
        mv.visitEnd();

        mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
        mv.visitCode();
        mv.visitVarInsn(ALOAD, 0);
        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
        mv.visitInsn(RETURN);
        mv.visitMaxs(1, 1);
        mv.visitEnd();

        cw.visitEnd();
        byte[] bytes = cw.toByteArray();

        return generator.defineClass(className, cw.toByteArray(), 0, bytes.length);
    }

    static Class<?> generateTestInstanceStrictFieldSetBeforeSuperclassInit() {
        String className = "TestInstanceStrictFieldSetBeforeSuperclassInit";
        return generateTestInstanceStrictField(className, true, false);
    }

    static Class<?> generateTestInstanceStrictFieldNotSetBeforeSuperclassInit() {
        String className = "TestInstanceStrictFieldNotSetBeforeSuperclassInit";
        return generateTestInstanceStrictField(className, false, false);
    }

    static Class<?> generateTestInstanceStrictFieldNotSetBeforeSuperclassInitMulti() {
        String className = "TestInstanceStrictFieldNotSetBeforeSuperclassInitMulti";
        return generateTestInstanceStrictField(className, true, true);
    }

    private static Class<?> generateTestInstanceStrictField(String className, boolean setField, boolean multipleFields) {
        String fieldName = "i";
        String fieldDesc = "I";
        String fieldName2 = "i2";
        String fieldName3 = "i3";

        ClassWriter cw = new ClassWriter(0);
        cw.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION,
            ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY,
            className, null, "java/lang/Object", null);

        cw.visitField(ValhallaUtils.ACC_STRICT_INIT, fieldName, fieldDesc, null, null);
        if (multipleFields) {
            cw.visitField(ValhallaUtils.ACC_STRICT_INIT, fieldName2, fieldDesc, null, null);
            cw.visitField(ValhallaUtils.ACC_STRICT_INIT, fieldName3, fieldDesc, null, null);
        }

        MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
        mv.visitCode();
        if (setField) {
            mv.visitVarInsn(ALOAD, 0);
            mv.visitInsn(ICONST_0);
            mv.visitFieldInsn(PUTFIELD, className, fieldName, fieldDesc);
        }
        mv.visitVarInsn(ALOAD, 0);
        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
        mv.visitInsn(RETURN);
        int maxStack = 1;
        if (setField) {
            maxStack = 2;
        }
        mv.visitMaxs(maxStack, 1);
        mv.visitEnd();

        cw.visitEnd();
        byte[] bytes = cw.toByteArray();
        return generator.defineClass(className, cw.toByteArray(), 0, bytes.length);
    }

    static Class<?> testInstanceStrictFinalFieldSetAfterThisInit() {
        String className = "TestInstanceStrictFinalFieldSetAfterThisInit";
        return generateTestInstanceStrictFieldThisInit(className, true);
    }

    static Class<?> generateTestInstanceStrictNonFinalFieldSetAfterThisInit() {
        String className = "TestInstanceStrictNonFinalFieldSetAfterThisInit";
        return generateTestInstanceStrictFieldThisInit(className, false);
    }

    private static Class<?> generateTestInstanceStrictFieldThisInit(String className, boolean finalField) {
        String fieldName = "i";
        String fieldDesc = "I";
        int fieldFlags = ValhallaUtils.ACC_STRICT_INIT | (finalField ? ACC_FINAL : 0);

        ClassWriter cw = new ClassWriter(0);
        cw.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION,
            ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY,
            className, null, "java/lang/Object", null);

        cw.visitField(fieldFlags, fieldName, fieldDesc, null, null);

        /* <init> calls <init>(I) to do the field initialization. */
        MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
        mv.visitCode();
        mv.visitVarInsn(ALOAD, 0);
        mv.visitIntInsn(BIPUSH, 0);
        mv.visitMethodInsn(INVOKESPECIAL, className, "<init>", "(I)V", false);
        mv.visitVarInsn(ALOAD, 0);
        mv.visitVarInsn(BIPUSH, 1);
        mv.visitFieldInsn(PUTFIELD, className, fieldName, fieldDesc);
        mv.visitInsn(RETURN);
        mv.visitMaxs(2, 1);
        mv.visitEnd();

        /* <init>(I) does the field initialization and then calls superclass's <init>. */
        mv = cw.visitMethod(ACC_PUBLIC, "<init>", "(I)V", null, null);
        mv.visitVarInsn(ALOAD, 0);
        mv.visitVarInsn(ILOAD, 1);
        mv.visitFieldInsn(PUTFIELD, className, fieldName, fieldDesc);
        mv.visitVarInsn(ALOAD, 0);
        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
        mv.visitInsn(RETURN);
        mv.visitMaxs(2, 2);
        mv.visitEnd();

        cw.visitEnd();
        byte[] bytes = cw.toByteArray();
        return generator.defineClass(className, cw.toByteArray(), 0, bytes.length);
    }

    public static Class<?> generateTestIfNonNullUninitializedThis() {
	    String className = "TestIfNonNullUninitializedThis";
	    return generateUninitializedValueField(className, false, true, false, false);
    }

    public static Class<?> generateTestIfNullUninitializedThis() {
	    String className = "TestIfNullUninitializedThis";
	    return generateUninitializedValueField(className, true, false, false, false);
    }

    public static Class<?> generateTestIfAcmpeqUninitializedValue() {
	    String className = "TestIfAcmpeqUninitializedValue";
	    return generateUninitializedValueField(className, false, false, true, false);
    }

    public static Class<?> generateTestIfAcmpneUninitializedValue() {
	    String className = "TestIfAcmpneUninitializedValue";
	    return generateUninitializedValueField(className, false, false, false, true);
    }

    private static Class<?> generateUninitializedValueField(String className, boolean ifNull, boolean ifNonNull, boolean ifAcmpeq, boolean ifAcmpne) {
	    ClassWriter cw = new ClassWriter(0);
	    cw.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION,
			    ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY,
			    className, null, "java/lang/Object", null);
	    MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
	    mv.visitCode();
	    Label label = new Label();

	    if (ifNull || ifNonNull) {
		    mv.visitVarInsn(ALOAD, 0);
		    if (ifNull) {
			    mv.visitJumpInsn(IFNULL, label);
		    } else {
			    mv.visitJumpInsn(IFNONNULL, label);
		    }
		    mv.visitInsn(NOP);
	    }

	     if (ifAcmpeq || ifAcmpne) {
		     mv.visitTypeInsn(NEW, "java/lang/Object");
		     mv.visitInsn(DUP);
		     mv.visitVarInsn(ALOAD, 0);
		     if (ifAcmpeq) {
			     mv.visitJumpInsn(IF_ACMPEQ, label);
		     } else {
			     mv.visitJumpInsn(IF_ACMPNE, label);
		     }
		     mv.visitInsn(NOP);
	     }
	     mv.visitLabel(label);
	     mv.visitVarInsn(ALOAD, 0);
	     mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
	     mv.visitInsn(RETURN);
	     mv.visitMaxs(3, 1);
	     mv.visitEnd();
	     cw.visitEnd();
	     byte[] bytes = cw.toByteArray();
	     return generator.defineClass(className, bytes, 0, bytes.length);
    }
}
