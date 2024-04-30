package org.openj9.test.hiddenclasses;

/*
 * Copyright IBM Corp. and others 2022
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

import jdk.internal.misc.Unsafe;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.MethodVisitor;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.lang.reflect.Field;

import static org.objectweb.asm.Opcodes.*;

@Test(groups = { "level.sanity" })
public class HiddenClassFinalFieldTests {

    static Unsafe unsafe = Unsafe.getUnsafe();

    Class<?> makeHiddenClass() throws Exception {
        MethodHandles.Lookup lookup = MethodHandles.lookup();

        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        FieldVisitor fv;
        MethodVisitor mv;
        cw.visit(V1_8, ACC_PUBLIC | ACC_SUPER, "org/openj9/test/hiddenclasses/TestHiddenClass", null, "java/lang/Object", null);
        {
            fv = cw.visitField(ACC_PUBLIC | ACC_STATIC, "modifiableField", "Ljava/lang/String;", null, "old");
            fv.visitEnd();
        }
        {
            cw.visitField(ACC_PUBLIC | ACC_STATIC | ACC_FINAL, "finalField", "Ljava/lang/String;", null, "old");
            fv.visitEnd();
        }
        {
            mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
            mv.visitCode();
            mv.visitVarInsn(ALOAD, 0);
            mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
            mv.visitInsn(RETURN);
            mv.visitMaxs(1, 1);
            mv.visitEnd();
        }
        cw.visitEnd();

        return lookup.defineHiddenClass(cw.toByteArray(), true, MethodHandles.Lookup.ClassOption.NESTMATE).lookupClass();
    }

    /* Final static fields in hidden classes are not modifiable through reflection. */
    @Test(expectedExceptions = java.lang.IllegalAccessException.class)
    public void test_javaLangReflectFieldSet_finalStaticHiddenClassField() throws Throwable {
        Class<?> c = makeHiddenClass();

        Object hiddenClassObject = c.getConstructor().newInstance();
        Field finalHiddenClassField = hiddenClassObject.getClass().getDeclaredField("finalField");
        finalHiddenClassField.setAccessible(true);
        finalHiddenClassField.set(hiddenClassObject, "new");
    }

    /* Make sure non-final fields in hidden classes can still be modified. */
    @Test
    public void test_javaLangReflectFieldSet_StaticHiddenClassField() throws Throwable {
        Class<?> c = makeHiddenClass();

        Object hiddenClassObject = c.getConstructor().newInstance();
        Field finalHiddenClassField = hiddenClassObject.getClass().getDeclaredField("modifiableField");
        finalHiddenClassField.setAccessible(true);
        finalHiddenClassField.set(hiddenClassObject, "new");
    }

    /* Check that Unsafe.staticFieldOffset supports hidden classes. */
    @Test
    public void test_jdkInternalMiscUnsafe_staticFieldOffset() throws Throwable {
        Class<?> c = makeHiddenClass();
        Field finalHiddenClassField = c.getDeclaredField("finalField");
        unsafe.staticFieldOffset(finalHiddenClassField);
    }

    /* Check that Unsafe.staticFieldBase supports hidden classes. */
    @Test
    public void test_jdkInternalMiscUnsafe_staticFieldBase() throws Throwable {
        Class<?> c = makeHiddenClass();
        Field finalHiddenClassField = c.getDeclaredField("finalField");
        unsafe.staticFieldBase(finalHiddenClassField);
    }

    /* VarHandle.set is not supported for hidden classes. */
    @Test(expectedExceptions = java.lang.UnsupportedOperationException.class)
    public void test_javaLangInvokeMethodHandles_unreflectVarHandle_static() throws Throwable {
        Class<?> c = makeHiddenClass();
        Field finalHiddenClassField = c.getDeclaredField("finalField");
        finalHiddenClassField.setAccessible(true);
        VarHandle finalHiddenClassFieldHandle = MethodHandles.lookup().unreflectVarHandle(finalHiddenClassField);
        finalHiddenClassFieldHandle.set("new");
    }
}
