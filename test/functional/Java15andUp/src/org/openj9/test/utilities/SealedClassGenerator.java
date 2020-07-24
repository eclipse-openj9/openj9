package org.openj9.test.utilities;

/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

 import org.objectweb.asm.*;

 public class SealedClassGenerator implements Opcodes {
    private static String dummySubclassName = "TestSubclassGen";

    public static byte[] generateFinalSealedClass(String className) {
        int accessFlags = ACC_FINAL | ACC_SUPER;

        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
        cw.visit(V15 | V_PREVIEW, accessFlags, className, null, "java/lang/Object", null);

        /* Sealed class must have a PermittedSubclasses attribute */
        cw.visitPermittedSubclass(dummySubclassName);

        cw.visitEnd();
        return cw.toByteArray();
    }

    public static byte[] generateSubclassIllegallyExtendingSealedSuperclass(String className, Class<?> superClass) {
        String superName = superClass.getName().replace('.', '/');
        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
        cw.visit(V15 | V_PREVIEW, ACC_SUPER, className, null, superName, null);
        cw.visitEnd();
        return cw.toByteArray();
    }

    public static byte[] generateSubinterfaceIllegallyExtendingSealedSuperinterface(String className, Class<?> superInterface) {
        String[] interfaces = { superInterface.getName().replace('.', '/') };
        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
        cw.visit(V15 | V_PREVIEW, ACC_SUPER, className, null, "java/lang/Object", interfaces);
        cw.visitEnd();
        return cw.toByteArray();
    }

    public static byte[] generateSealedClass(String className, String[] permittedSubclassNames) {
        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
        cw.visit(V15 | V_PREVIEW, ACC_SUPER, className, null, "java/lang/Object", null);

        for (String psName : permittedSubclassNames) {
            cw.visitPermittedSubclass(psName);
        }

        cw.visitEnd();
        return cw.toByteArray();
    }
 }
