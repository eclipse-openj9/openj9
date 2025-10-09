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

    // ACC_STRICT_INIT is not yet supported by ASM
    private static final int ACC_STRICT_INIT = ACC_STRICT;

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

        cw.visitField(ACC_STRICT_INIT | ACC_FINAL, fieldName, fieldDesc, null, null);

        MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
        mv.visitCode();
        mv.visitVarInsn(ALOAD, 0);
        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
        if (lateLarval) {
            mv.visitVarInsn(ALOAD, 0);
            mv.visitInsn(ICONST_0);
            mv.visitFieldInsn(PUTFIELD, className, fieldName, fieldDesc);
        }
        mv.visitInsn(RETURN);
        mv.visitMaxs(lateLarval ? 2 : 1, 1);
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

    private static Class<?> generateTestPutGetStrictStaticField(String className, boolean secondField, boolean put1, boolean get, boolean put2) {
        String fieldName = "i";
        String fieldDesc = "I";
        String fieldName2 = "i2";

        ClassWriter cw = new ClassWriter(0);
        cw.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION,
            ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY,
            className, null, "java/lang/Object", null);

        cw.visitField(ACC_STATIC | ACC_STRICT_INIT | ACC_FINAL, fieldName, fieldDesc, null, null);
        if (secondField) {
            cw.visitField(ACC_STATIC | ACC_STRICT_INIT | ACC_FINAL, fieldName2, fieldDesc, null, null);
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
}
