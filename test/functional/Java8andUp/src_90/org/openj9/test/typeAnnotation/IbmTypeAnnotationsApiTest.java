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
import org.testng.annotations.BeforeMethod;
import java.io.ByteArrayInputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;


import com.ibm.oti.reflect.TypeAnnotationParser;

import jdk.internal.misc.SharedSecrets;
import jdk.internal.misc.JavaLangAccess;

/**
 * Sanity checking only; no checking of the returned data.
 *
 */
@Test(groups = { "level.sanity" })
public class IbmTypeAnnotationsApiTest {

	@SuppressWarnings("rawtypes")
	private Class<? extends TypeAnnotatedTestClass> typeAnnotatedClass;

	@Test
	public void testGetMethodTypeAnnotationsAttribute() {
		for (Method m: typeAnnotatedClass.getDeclaredMethods()) {
			String mName = m.getName();
			byte[] attr = TypeAnnotationParser.getTypeAnnotationsData(m);
			TypeAnnotationUtils.dumpTypeAnnotation("method", mName, typeAnnotatedClass, attr);
		}
	}

	@Test
	public void testGetFieldTypeAnnotationsAttribute() {
		for (Field f: typeAnnotatedClass.getDeclaredFields()) {
			String fName = f.getName();
			byte[] attr = TypeAnnotationParser.getTypeAnnotationsData(f);
			TypeAnnotationUtils.dumpTypeAnnotation("field", fName, typeAnnotatedClass, attr);
		}
	}

	@Test
	public void testGetClassTypeAnnotationsAttribute() {
		byte[] attr = TypeAnnotationParser.getTypeAnnotationsData(typeAnnotatedClass);
		TypeAnnotationUtils.dumpTypeAnnotation("class", typeAnnotatedClass.getName(), typeAnnotatedClass, attr);
	}

	@Test
	public void testGetRawClassTypeAnnotations() {
		JavaLangAccess jlAccess = SharedSecrets.getJavaLangAccess();
		byte attr[] = jlAccess.getRawClassTypeAnnotations(typeAnnotatedClass);
		TypeAnnotationUtils.dumpTypeAnnotation("class", typeAnnotatedClass.getName(), typeAnnotatedClass, attr);
	}

	@Test
	public void testGetRawMethodTypeAnnotations() {
		JavaLangAccess jlAccess = SharedSecrets.getJavaLangAccess();
		for (Method m: typeAnnotatedClass.getDeclaredMethods()) {
			byte attr[] = jlAccess.getRawExecutableTypeAnnotations(m);
			if (null != attr) {
				TypeAnnotationUtils.dumpTypeAnnotation("class", typeAnnotatedClass.getName(), typeAnnotatedClass, attr);
			}
		}
	}

	@Test
	public void testGetRawConstructorTypeAnnotations() {
		JavaLangAccess jlAccess = SharedSecrets.getJavaLangAccess();
		for (Constructor<?> c: typeAnnotatedClass.getDeclaredConstructors()) {
			byte attr[] = jlAccess.getRawExecutableTypeAnnotations(c);
			if (null != attr) {
				TypeAnnotationUtils.dumpTypeAnnotation("class", typeAnnotatedClass.getName(), typeAnnotatedClass, attr);
			}
		}
	}

	@Test
	public void testGetRawClassAnnotations() {
		JavaLangAccess jlAccess = SharedSecrets.getJavaLangAccess();
		byte attr[] = jlAccess.getRawClassAnnotations(typeAnnotatedClass);
		ByteArrayInputStream attrReader = new ByteArrayInputStream(attr);
		TypeAnnotationUtils.dumpAnnotations(attrReader);
	}

	@BeforeMethod
	protected void setUp() throws Exception {
		typeAnnotatedClass = TypeAnnotatedTestClass.class;
	}
}
