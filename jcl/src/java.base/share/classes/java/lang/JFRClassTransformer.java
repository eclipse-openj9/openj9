/*[INCLUDE-IF JAVA_SPEC_VERSION == 17]*/
/*
 * Copyright IBM Corp. and others 1998
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
package java.lang;

import jdk.internal.org.objectweb.asm.*;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

final class JFRClassTransformer {

    /*[IF JAVA_SPEC_VERSION == 17]*/
    private static Method bytesForEagerInstrumentation;

    static {
        try {
            Class<?> jfrUpCallClass = Class.forName("jdk.jfr.internal.JVMUpcalls");
            bytesForEagerInstrumentation = jfrUpCallClass.getDeclaredMethod("bytesForEagerInstrumentation", long.class, boolean.class, Class.class, byte[].class);
            bytesForEagerInstrumentation.setAccessible(true);
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }
    /*[ENDIF] JAVA_SPEC_VERSION == 17 */

    private static final String EVENT_HANDLER_FIELD = "eventHandler";
    private static final String START_TIME_FIELD = "startTime";
    private static final String DURATION_FIELD = "duration";
    private static final String OBJECT_DESC = "Ljava/lang/Object;";
    private static final String LONG_DESC = "J";

    private static class FieldDescriptor {
        public final String name;
        public final String descriptor;
        public final int access;

        public FieldDescriptor(String name, String descriptor, int access) {
            this.name = name;
            this.descriptor = descriptor;
            this.access = access;
        }

        public static FieldDescriptor staticField(String name, String descriptor) {
            return new FieldDescriptor(name, descriptor,
                Opcodes.ACC_PRIVATE | Opcodes.ACC_STATIC | Opcodes.ACC_SYNTHETIC);
        }

        public static FieldDescriptor transientField(String name, String descriptor) {
            return new FieldDescriptor(name, descriptor,
                Opcodes.ACC_PRIVATE | Opcodes.ACC_TRANSIENT | Opcodes.ACC_SYNTHETIC);
        }

        public static FieldDescriptor instanceField(String name, String descriptor) {
            return new FieldDescriptor(name, descriptor,
                Opcodes.ACC_PRIVATE | Opcodes.ACC_SYNTHETIC);
        }
    }

    private static class MethodDescriptor {
        public final String name;
        public final String descriptor;
        public final int access;
        public final MethodGenerator generator;

        public MethodDescriptor(String name, String descriptor, int access, MethodGenerator generator) {
            this.name = name;
            this.descriptor = descriptor;
            this.access = access;
            this.generator = generator;
        }

        public static MethodDescriptor publicMethod(String name, String descriptor, MethodGenerator generator) {
            return new MethodDescriptor(name, descriptor, Opcodes.ACC_PUBLIC, generator);
        }
    }

    @FunctionalInterface
    private interface MethodGenerator {
        void generate(MethodVisitor mv);
    }

    private static byte[] addFieldsAndMethods(byte[] classBytes, List<FieldDescriptor> fields, List<MethodDescriptor> methods) {
        ClassReader reader = new ClassReader(classBytes);
        ClassWriter writer = new ClassWriter(reader, ClassWriter.COMPUTE_MAXS);

        ClassVisitor visitor = new ClassVisitor(Opcodes.ASM7, writer) {
            @Override
            public void visitEnd() {
                for (FieldDescriptor field : fields) {
                    FieldVisitor fv = super.visitField(field.access, field.name, field.descriptor, null, null);
                    if (fv != null) {
                        fv.visitEnd();
                    }
                }

                for (MethodDescriptor method : methods) {
                    MethodVisitor mv = super.visitMethod(method.access, method.name, method.descriptor, null, null);
                    if (mv != null) {
                        mv.visitCode();
                        method.generator.generate(mv);
                        mv.visitEnd();
                    }
                }

                super.visitEnd();
            }
        };

        reader.accept(visitor, 0);
        return writer.toByteArray();
    }

	/**
	 * Transforms the class bytes to add the JFR event handler fields and methods. Adding methods
	 * is optional and depends on the super class of the class being transformed.
	 *
	 * @param classBytes the class bytes to transform
	 * @param addMethods whether to add the JFR event handler methods
	 * @return the transformed class bytes
	 */
    static byte[] transformClass(byte[] classBytes) {
        List<FieldDescriptor> fields = new ArrayList<>();

        fields.add(FieldDescriptor.staticField(EVENT_HANDLER_FIELD, OBJECT_DESC));

        fields.add(FieldDescriptor.transientField(START_TIME_FIELD, LONG_DESC));

        fields.add(FieldDescriptor.transientField(DURATION_FIELD, LONG_DESC));

        List<MethodDescriptor> methods = new ArrayList<>();

        methods.add(MethodDescriptor.publicMethod("isEnabled", "()Z", mv -> {
            mv.visitInsn(Opcodes.ICONST_0);
            mv.visitInsn(Opcodes.IRETURN);
            mv.visitMaxs(1, 1);
        }));

        methods.add(MethodDescriptor.publicMethod("shouldCommit", "()Z", mv -> {
            mv.visitInsn(Opcodes.ICONST_0);
            mv.visitInsn(Opcodes.IRETURN);
            mv.visitMaxs(1, 1);
        }));

        methods.add(MethodDescriptor.publicMethod("commit", "()V", mv -> {
            mv.visitInsn(Opcodes.RETURN);
            mv.visitMaxs(0, 1);
        }));

        methods.add(MethodDescriptor.publicMethod("end", "()V", mv -> {
            mv.visitInsn(Opcodes.RETURN);
            mv.visitMaxs(0, 1);
        }));

        methods.add(MethodDescriptor.publicMethod("begin", "()V", mv -> {
            mv.visitInsn(Opcodes.RETURN);
            mv.visitMaxs(0, 1);
        }));

        return addFieldsAndMethods(classBytes, fields, methods);
    }

    /*[IF JAVA_SPEC_VERSION == 17]*/
    static byte[] transformJFREventClass(byte[] classBytes) {
        ClassReader reader = new ClassReader(classBytes);
        ClassWriter writer = new ClassWriter(reader, ClassWriter.COMPUTE_MAXS);

        ClassVisitor visitor = new ClassVisitor(Opcodes.ASM7, writer) {
            @Override
            public MethodVisitor visitMethod(int access, String name, String descriptor, String signature, String[] exceptions) {
                // Remove the ACC_FINAL flag from the method access modifiers
                int modifiedAccess = access & ~Opcodes.ACC_FINAL;
                return super.visitMethod(modifiedAccess, name, descriptor, signature, exceptions);
            }
        };

        reader.accept(visitor, 0);
        return writer.toByteArray();
    }

    static byte[] transformClassAndInvokebytesForEagerInstrumentation(long traceId, boolean forceInstrumentation, Class<?> superClass, byte[] oldBytes) throws ReflectiveOperationException {
        try {
            oldBytes = transformClass(oldBytes);
            return (byte[])bytesForEagerInstrumentation.invoke(null, traceId, forceInstrumentation, superClass, oldBytes);
        } catch (Throwable t) {
            t.printStackTrace();
            throw t;
        }
    }
    /*[ENDIF] JAVA_SPEC_VERSION == 17 */
}
