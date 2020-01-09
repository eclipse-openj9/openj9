package org.openj9.test.java.lang;

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

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import jdk.jfr.Timestamp;

import org.testng.AssertJUnit;

import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.AnnotatedType;
import java.lang.reflect.Field;
import java.lang.reflect.RecordComponent;

import org.openj9.test.utilities.RecordClassGenerator;
import org.openj9.test.utilities.CustomClassLoader;

/**
 * Test JCL additions to java.lang.Class from JEP 359: Records preview.
 * 
 * New methods include:
 * - boolean isRecord()
 * - Object[] getRecordComponents()
 */

 @Test(groups = { "level.sanity" })
 public class Test_Class {
    private static final Logger logger = Logger.getLogger(Test_Class.class);

    /* Test classes and records */
    class TestClass {}
    record TestRecordEmpty() {}
    record TestRecordPrim(int x, long y) {}
    record TestRecordObj(String x, Double y, Object[] z) {}

    /* constants for asm tests */
    String name = "TestRecordComponents";
    String rcName = "x";
    String rcType = "I";
    String rcSignature = "I";
    
    @Retention(RetentionPolicy.RUNTIME)
    @Inherited()
    public @interface TestAnnotation {}

    @Retention(RetentionPolicy.RUNTIME)
    @Target(ElementType.TYPE_USE)
    public @interface TestTypeAnnotation {}

    @Test
    public void test_isRecord() {
        AssertJUnit.assertTrue("TestRecordEmpty is a record", TestRecordEmpty.class.isRecord());
        AssertJUnit.assertTrue("TestClass is not a record", !TestClass.class.isRecord());
    }

    @Test
    public void test_getRecordComponent_nonRecordClass() throws Throwable {
        RecordComponent[] rc = TestClass.class.getRecordComponents();
        AssertJUnit.assertNull(rc);
    }

    @Test
    public void test_getRecordComponent_recordWithNoComponents() throws Throwable {
        /* record with no components */
        RecordComponent[] rc1 = TestRecordEmpty.class.getRecordComponents();
        AssertJUnit.assertEquals(0, rc1.length);
    }

    @Test
    public void test_getRecordComponent_noOptionalAttributes() throws Throwable {
        /* record with primitive type components */
        RecordComponent[] rc2 = TestRecordPrim.class.getRecordComponents();
        AssertJUnit.assertEquals(2, rc2.length);
        test_RecordComponentContents_noAttributes(rc2[0], TestRecordPrim.class, "x", int.class);
        test_RecordComponentContents_noAttributes(rc2[1], TestRecordPrim.class, "y", long.class);

        /* record with Object components */
        RecordComponent[] rc3 = TestRecordObj.class.getRecordComponents();
        AssertJUnit.assertEquals(3, rc3.length);
        test_RecordComponentContents_noAttributes(rc3[0], TestRecordObj.class, "x", String.class);
        test_RecordComponentContents_noAttributes(rc3[1], TestRecordObj.class, "y", Double.class);
        test_RecordComponentContents_noAttributes(rc3[2], TestRecordObj.class, "z", Object[].class);
    }

    @Test
    public void test_getRecordComponent_signatureAttribute() throws Throwable {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes = RecordClassGenerator.generateRecordAttributes(name, rcName, rcType, rcSignature, null, null);
        Class<?> clazz = classloader.getClass(name, bytes);

        RecordComponent[] rc = clazz.getRecordComponents();
        AssertJUnit.assertEquals(1, rc.length);
        test_RecordComponentContents(rc[0], clazz, rcName, int.class, rcSignature, null, null);
    }

    @Test
    public void test_getRecordComponent_annotationsAttribute() throws Throwable {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes = RecordClassGenerator.generateRecordAttributes(name, rcName, rcType, null, 
            TestAnnotation.class.descriptorString(), null);
        Class<?> clazz = classloader.getClass(name, bytes);

        RecordComponent[] rc = clazz.getRecordComponents();
        AssertJUnit.assertEquals(1, rc.length);
        test_RecordComponentContents(rc[0], clazz, rcName, int.class, null, TestAnnotation.class, null);
    }

    @Test
    public void test_getRecordComponent_typeAnnotationsAttribute() throws Throwable {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes = RecordClassGenerator.generateRecordAttributes(name, rcName, rcType, null, 
            null, TestTypeAnnotation.class.descriptorString());
        Class<?> clazz = classloader.getClass(name, bytes);

        RecordComponent[] rc = clazz.getRecordComponents();
        AssertJUnit.assertEquals(1, rc.length);
        test_RecordComponentContents(rc[0], clazz, rcName, int.class, null, null, TestTypeAnnotation.class);
    }

    @Test
    public void test_getRecordComponent_multipleAttributes() throws Throwable {
        CustomClassLoader classloader = new CustomClassLoader();
        /* generate a record class with all three types of optional attributes */
        byte[] bytes = RecordClassGenerator.generateRecordAttributes(name, rcName, rcType, rcSignature, 
            TestAnnotation.class.descriptorString(), TestTypeAnnotation.class.descriptorString());
        Class<?> clazz = classloader.getClass(name, bytes);

        RecordComponent[] rc = clazz.getRecordComponents();
        AssertJUnit.assertEquals(1, rc.length);
        test_RecordComponentContents(rc[0], clazz, rcName, int.class, rcSignature, TestAnnotation.class, TestTypeAnnotation.class);
    }

    @Test
    public void test_getRecordComponent_securityException() throws Throwable {
        // TODO
    }

    private void test_RecordComponentContents_noAttributes(RecordComponent rc, Class<?> declaringRecord, String name, 
                    Class<?> type) throws Throwable {
        test_RecordComponentContents(rc, declaringRecord, name, type, null, null, null);
    }

    /* test all fields that are set from record_component_info attribute */
    private void test_RecordComponentContents(RecordComponent rc, Class<?> declaringRecord, String name, Class<?> type,
                    String signature, Class annotationsClass, Class typeAnnotationsClass) throws Throwable {
        logger.info("test components for component " + name + " in record " + declaringRecord.toString());

        AssertJUnit.assertEquals(declaringRecord, rc.getDeclaringRecord());
        AssertJUnit.assertEquals(name, rc.getName());
        AssertJUnit.assertEquals(type, rc.getType());
        AssertJUnit.assertEquals(declaringRecord.getMethod(name), rc.getAccessor());

        /* check signature attribte */
        AssertJUnit.assertEquals(signature, rc.getGenericSignature());

        /* check annotations attributes */
        if (null == annotationsClass) {
            /* RecordComponent should have no annotation data */
            Field annotationsField = RecordComponent.class.getDeclaredField("annotations");
            annotationsField.setAccessible(true);
            byte[] annotations = (byte[])annotationsField.get(rc);
            AssertJUnit.assertNull(annotations);
        } else {
            AssertJUnit.assertNotNull(rc.getAnnotation(annotationsClass));
        }

        /* check type annotations attribute */
        if (null == typeAnnotationsClass) {
            /* RecordComponent should have no type annotation data */
            Field typeAnnotationsField = RecordComponent.class.getDeclaredField("typeAnnotations");
            typeAnnotationsField.setAccessible(true);
            byte[] typeAnnotations = (byte[])typeAnnotationsField.get(rc);
            AssertJUnit.assertNull(typeAnnotations);
        } else {
            AnnotatedType annotatedType = rc.getAnnotatedType();
            AssertJUnit.assertNotNull(annotatedType);
        }
    }
 }
 