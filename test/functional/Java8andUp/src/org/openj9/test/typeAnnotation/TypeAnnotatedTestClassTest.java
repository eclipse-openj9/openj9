package org.openj9.test.typeAnnotation;
/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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
import org.testng.AssertJUnit;
import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.AnnotatedType;
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.TypeVariable;
import java.util.HashMap;
import java.util.HashSet;


/**
 * Tests all type annotation APIs,
 * type annotation classfile datastructure variants, and JVM code paths.
 * Note that several Java source-code usages of type annotations map to the same
 * classfile datastructure variants.
 *
 */
@Test(groups = { "level.sanity" })
public class TypeAnnotatedTestClassTest {

	private static final Logger logger = Logger.getLogger(TypeAnnotatedTestClassTest.class);
	private static final String ANNOTATION_ARG_NAME = "site";
	private static final String ANNOTATION_NAME = "org.openj9.test.typeAnnotation.TestAnn";
	@SuppressWarnings("rawtypes")
	public static final Class<? extends TypeAnnotatedTestClass> typeAnnotatedClass = TypeAnnotatedTestClass.class;
	@SuppressWarnings("rawtypes")
	public static final Class annotatedTypeParamsClass = AnnotatedTypeParameterTestClass.class;
	HashMap<String, String[]> fieldTypeAnnotations;
	HashMap<String, String[]> returnTypeAnnotations;
	HashMap<String, String[]> receiverTypeAnnotations;
	HashMap<String, String[]> exceptionTypeAnnotations;
	HashMap<String, String[]> parameterTypeAnnotations;
	HashMap<String, String[]> typeParametersAnnotations;
	HashMap<String, String[]> typeBoundsAnnotations;
	HashMap<String, String[]> methodTypeBoundAnnotations;
	HashMap<String, String[]> methodGenericAnnotations;
	HashMap<String, String[]> constructorAnnotations;

	static final boolean verbose = false;


	@Test
	public void testSanity() {
		Class<?> c = typeAnnotatedClass;
		Annotation[] annotations = c.getAnnotations();
		if (verbose) {
			TypeAnnotationUtils.printAnnotationList("class annotations", annotations);
		}
		AssertJUnit.assertEquals("wrong number of classic assertions", 1, annotations.length);
	}

	@Test
	public void testSuperclassAnnotations() {
		Class<?> c = typeAnnotatedClass;
		checkAnnotations(c.getAnnotatedSuperclass(), ANNOTATION_NAME, ANNOTATION_ARG_NAME, new String[] {"extendsAnnotation"});
	}

	@Test
	public void testFieldAnnotations() {
		initializeFieldTypeAnnotations();
		for (Field f: typeAnnotatedClass.getFields()) {
			checkAnnotations(f.getAnnotatedType(), ANNOTATION_NAME, ANNOTATION_ARG_NAME, fieldTypeAnnotations.get(f.getName()));
		}
	}

	@Test
	public void testInterfaceAnnotations() {
		Class<?> c = typeAnnotatedClass;
		AnnotatedType[] annotatedInterfaces = c.getAnnotatedInterfaces();
		checkAnnotations(annotatedInterfaces, ANNOTATION_NAME, ANNOTATION_ARG_NAME, new String[] {"implementsSerializableAnnotation", "implementsIterableAnnotation"});
	}

	@Test
	public void testMethodAnnotations() {
		Class<?> c = typeAnnotatedClass;
		initializeReturnTypeAnnotations();
		initializeReceiverTypeAnnotations();
		initializeExceptionTypeAnnotations();
		initializeParameterTypeAnnotations();
		Executable executables[] = c.getDeclaredMethods();
		for (Executable e: executables) {
			String execName = e.getName();
			if (verbose) {
				logger.debug("checking method "+execName);
			}
			checkAnnotations(e.getAnnotatedReturnType(), ANNOTATION_NAME, ANNOTATION_ARG_NAME, returnTypeAnnotations.get(execName));
			AnnotatedType annotatedReceiverType = e.getAnnotatedReceiverType();
			checkAnnotations(annotatedReceiverType, ANNOTATION_NAME, ANNOTATION_ARG_NAME, receiverTypeAnnotations.get(execName));
			checkAnnotations(e.getAnnotatedExceptionTypes(), ANNOTATION_NAME, ANNOTATION_ARG_NAME, exceptionTypeAnnotations.get(execName));
			checkAnnotations(e.getAnnotatedParameterTypes(), ANNOTATION_NAME, ANNOTATION_ARG_NAME, parameterTypeAnnotations.get(execName));
		}
	}

	@Test
	public void testMethodTypeBoundAnnotations() {
		initializeMethodTypeBoundAnnotations();
		initializeMethodGenericAnnotations();
		for (Executable e: typeAnnotatedClass.getDeclaredMethods()) {
			for (@SuppressWarnings("rawtypes") TypeVariable tv: e.getTypeParameters()) {
				checkAnnotations(tv, ANNOTATION_NAME, ANNOTATION_ARG_NAME, methodGenericAnnotations.get(tv.getName()));
				for (AnnotatedType b: tv.getAnnotatedBounds()) {
					checkAnnotations(b, ANNOTATION_NAME, ANNOTATION_ARG_NAME, methodTypeBoundAnnotations.get(b.getType().getTypeName()));
				}
			}
		}
	}

	@Test
	public void testTypeBoundsTypeAnnotations_1() {
		Class<?> c = typeAnnotatedClass;
		initializeTypeParametersAnnotations();
		initializeTypeBoundsAnnotations();
		for (TypeVariable<?> tp: c.getTypeParameters()) {
			String tpName = tp.getName();
			checkAnnotations(tp.getAnnotatedBounds(), ANNOTATION_NAME, ANNOTATION_ARG_NAME, typeBoundsAnnotations.get(tpName));
		}
	}

	@Test
	public void testTypeBoundsTypeAnnotations_2() {
		Class<?> c = annotatedTypeParamsClass;
		initializeTypeParametersAnnotations();
		initializeTypeBoundsAnnotations();
		for (TypeVariable<?> tp: c.getTypeParameters()) {
			String tpName = tp.getName();
			if (verbose) {
				logger.debug("type parameters annotations for "+tp.getName());
				for (Annotation an: tp.getAnnotations()) {
					logger.debug(an);
				}
			}
			checkAnnotations(tp, ANNOTATION_NAME, ANNOTATION_ARG_NAME, typeParametersAnnotations.get(tpName));
			checkAnnotations(tp.getAnnotatedBounds(), ANNOTATION_NAME, ANNOTATION_ARG_NAME, typeBoundsAnnotations.get(tpName));
		}
	}

	@Test
	public void testConstructorTypeAnnotations() {
		Class<?> c = typeAnnotatedClass;
		initializeConstructorAnnotations();
		for (Constructor<?> con: c.getDeclaredConstructors()) {
			if (verbose) {
				logger.debug("constructor annotations for "+c.getName());
			}
			checkAnnotations(con.getAnnotatedParameterTypes(), ANNOTATION_NAME, ANNOTATION_ARG_NAME, constructorAnnotations.get(con.getName()));
		}
	}

	/**
	 * Verify that annotations are not re-used
	 */
	@Test
	public void testGetAnnotatedInterfacesSecurity() {
		Class<?> clazz = typeAnnotatedClass;
		AnnotatedType[] annIfs1 = clazz.getAnnotatedInterfaces();
		if (annIfs1.length > 0) {
			AnnotatedType[] annIfs2 = clazz.getAnnotatedInterfaces();
			annIfs1[0] = null;
			AssertJUnit.assertNotNull("Second instance of AnnotatedInterfaces should not be modified", annIfs2[0]);
		}
	}

	/* Utility code */
	private void checkAnnotations(AnnotatedElement annotatee, 	String annotationName, String argumentName, String[] expectedAnnotations) {
		AnnotatedElement[] singletonAnnotationType = null;
		if (null == annotatee) {
			singletonAnnotationType = new AnnotatedElement[0];
		} else {
			singletonAnnotationType = new AnnotatedElement[1];
			singletonAnnotationType[0] = annotatee;
		}
		checkAnnotations(singletonAnnotationType, annotationName, argumentName, expectedAnnotations);
	}

	private void checkAnnotations(AnnotatedElement[] annotatees,
			String annotationName, String argumentName, String[] expectedAnnotations) {
		HashSet<String> actualAnnotations = new HashSet<>(annotatees.length);
		for (AnnotatedElement at: annotatees) {
			for (Annotation an : at.getAnnotations()) {
				actualAnnotations.add(an.toString());
			}
		}
		if (verbose) {
			logger.debug("=================================");
			for (String s: actualAnnotations) {
				logger.debug("Annotation: "+s);
			}
		}
		if (null != expectedAnnotations) {
			AssertJUnit.assertEquals("wrong number of annotations", expectedAnnotations.length, actualAnnotations.size());
			for (String s: expectedAnnotations) {
				String comparandJava8 = "@"+annotationName+"("+argumentName+"="+s+")";
				String comparandJava9 = "@"+annotationName+"("+argumentName+"=\""+s+"\")";
				AssertJUnit.assertTrue(comparandJava8+" and "+comparandJava9+" missing", actualAnnotations.contains(comparandJava8) || actualAnnotations.contains(comparandJava9));
			}
		} else {
			AssertJUnit.assertTrue("unexpected annotations", actualAnnotations.size() == 0);
		}
	}

	private void initializeFieldTypeAnnotations() {
		fieldTypeAnnotations = new HashMap<>();
		fieldTypeAnnotations.put("fieldAndTypeAnnotatedField", new String[] {"fieldAndTypeAnnotatedField"});
	}

	private void initializeReturnTypeAnnotations() {
		returnTypeAnnotations = new HashMap<>();
		returnTypeAnnotations.put("myMethod", new String[] {"returnTypeAnnotation"});
	}

	private void initializeReceiverTypeAnnotations() {
		receiverTypeAnnotations = new HashMap<>();
		receiverTypeAnnotations.put("myTryCatch", new String[] {"receiverTypeAnnotation"});
	}

	private void initializeExceptionTypeAnnotations() {
		exceptionTypeAnnotations = new HashMap<>();
		exceptionTypeAnnotations.put("myMethod", new String[] {"throwsAnnotation"});
	}

	private void initializeParameterTypeAnnotations() {
		parameterTypeAnnotations = new HashMap<>();
		parameterTypeAnnotations.put("myMethod", new String[] {"formalParameterAnnotation"});
	}

	private void initializeTypeParametersAnnotations() {
		typeParametersAnnotations = new HashMap<>();
		typeParametersAnnotations.put("T", new String[] {"typeParameterAnnotation"});
	}

	private void initializeMethodTypeBoundAnnotations() {
		methodTypeBoundAnnotations = new HashMap<>();
		methodTypeBoundAnnotations.put("java.lang.Iterable", new String[] {"methodTypeBounds"});
	}

	private void initializeMethodGenericAnnotations() {
		methodGenericAnnotations = new HashMap<>();
		methodGenericAnnotations.put("E", new String[] {"methodGenericType"});
	}

	private void initializeTypeBoundsAnnotations() {
		typeBoundsAnnotations = new HashMap<>();
		typeBoundsAnnotations.put("T", new String[] {"methodTypeBounds"});
	}

	private void initializeConstructorAnnotations() {
		constructorAnnotations = new HashMap<>();
		constructorAnnotations.put("org.openj9.test.typeAnnotation.TypeAnnotatedTestClass", new String[] {"constructorAnnotation"});
	}
}
