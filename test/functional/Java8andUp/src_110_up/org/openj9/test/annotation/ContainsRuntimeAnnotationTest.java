package org.openj9.test.annotation;

/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.lang.reflect.*;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;

import java.util.Arrays;

import jdk.internal.reflect.ConstantPool;
import jdk.internal.vm.annotation.Stable;

@Test(groups = { "level.sanity" })
public class ContainsRuntimeAnnotationTest {
	private static final String THIS_CLASS_NAME = "org/openj9/test/annotation/ContainsRuntimeAnnotationTest";
	private final ConstantPool thisClassConstantPool;
	private final Method getConstantPool;
	private final Object javaLangAccess;

	static {
		try {
			System.loadLibrary("anntests");
		} catch (UnsatisfiedLinkError e) {
			Assert.fail(e.getMessage() + "\nlibrary path = " + System.getProperty("java.library.path"));
		}
	}

	@Stable
	int stableField = 0;

	int notStableField = 0;

	@Stable
	@MyFieldAnnotation
	@MyFieldAnnotation2
	@MyFieldAnnotation3
	@MyFieldAnnotation4
	int stable_MultipleAnnotations = 0;

	@MyFieldAnnotation
	@MyFieldAnnotation2
	@MyFieldAnnotation3
	@MyFieldAnnotation4
	int notStable_MultipleAnnotations = 0;

	public ContainsRuntimeAnnotationTest() throws Throwable {
		String version = System.getProperty("java.version");
		int dot = version.indexOf(".");
		if (dot != -1) {
			version = version.substring(0, dot);
		}

		Class SharedSecrets;
		Class JavaLangAccess;
		if (Integer.parseInt(version) == 11) {
			SharedSecrets = Class.forName("jdk.internal.misc.SharedSecrets");
			JavaLangAccess = Class.forName("jdk.internal.misc.JavaLangAccess");
		} else {
			SharedSecrets = Class.forName("jdk.internal.access.SharedSecrets");
			JavaLangAccess = Class.forName("jdk.internal.access.JavaLangAccess");
		}
		Method getJavaLangAccess = SharedSecrets.getDeclaredMethod("getJavaLangAccess");
		getConstantPool = JavaLangAccess.getDeclaredMethod("getConstantPool", new Class[] {Class.class});
		javaLangAccess = getJavaLangAccess.invoke(null);

		thisClassConstantPool = (ConstantPool)getConstantPool.invoke(javaLangAccess, this.getClass());
	}

	private static native boolean containsRuntimeAnnotation(Class clazz, int cpIndex, String annotationName, boolean isField, int type);

	@Test
	public void test_stable_annotation() throws Exception {
		boolean annotationFound;
		int cpIndex;
		final String stableAnnotation = "Ljdk/internal/vm/annotation/Stable;";

		/* resolve fields */
		int a = stableField;
		int b = notStableField;

		cpIndex = getMemberCPIndex(thisClassConstantPool, THIS_CLASS_NAME, "stableField", "I");
		Assert.assertTrue(-1 != cpIndex, "Could not find stableField");
		annotationFound = containsRuntimeAnnotation(ContainsRuntimeAnnotationTest.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertTrue(annotationFound, "stableField does not have Stable annotation");

		cpIndex = getMemberCPIndex(thisClassConstantPool, THIS_CLASS_NAME, "notStableField", "I");
		Assert.assertTrue(-1 != cpIndex, "Could not find notStableField");
		annotationFound = containsRuntimeAnnotation(ContainsRuntimeAnnotationTest.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertFalse(annotationFound, "notStableField has Stable annotation");
	}

	@Test
	public void test_multiple_annotations() throws Exception {
		boolean annotationFound;
		int cpIndex;
		final String stableAnnotation = "Ljdk/internal/vm/annotation/Stable;";

		/* resolve fields */
		int a = stable_MultipleAnnotations;
		int b = notStable_MultipleAnnotations;

		cpIndex = getMemberCPIndex(thisClassConstantPool, THIS_CLASS_NAME, "stable_MultipleAnnotations", "I");
		Assert.assertTrue(-1 != cpIndex, "Could not find stable_MultipleAnnotations");
		annotationFound = containsRuntimeAnnotation(ContainsRuntimeAnnotationTest.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertTrue(annotationFound, "stable_MultipleAnnotations does not have Stable annotation");

		cpIndex = getMemberCPIndex(thisClassConstantPool, THIS_CLASS_NAME, "notStable_MultipleAnnotations", "I");
		Assert.assertTrue(-1 != cpIndex, "Could not find notStable_MultipleAnnotations");
		annotationFound = containsRuntimeAnnotation(ContainsRuntimeAnnotationTest.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertFalse(annotationFound, "notStable_MultipleAnnotations has Stable annotation");
	}

	@Test
	public void test_field_in_external_class() throws Exception {
		boolean annotationFound = false;
		int cpIndex;
		final String stableAnnotation = "Ljdk/internal/vm/annotation/Stable;";

		/* resolve fields */
		C c = new C();
		c.method1(new B());

		ConstantPool constantPool = (ConstantPool)getConstantPool.invoke(javaLangAccess, C.class);
		final String cClassName = "org/openj9/test/annotation/C";
		final String bClassName = "org/openj9/test/annotation/B";

		cpIndex = getMemberCPIndex(constantPool, bClassName, "field1", "I");
		Assert.assertTrue(-1 != cpIndex, "couldn't find field1 fref");
		annotationFound = containsRuntimeAnnotation(C.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertTrue(annotationFound, "did not detect @Stable annotation");

		cpIndex = getMemberCPIndex(constantPool, bClassName, "field2", "I");
		Assert.assertTrue(-1 != cpIndex, "couldn't find field2 fref");
		annotationFound = containsRuntimeAnnotation(C.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertTrue(annotationFound, "did not detect @Stable annotation");

		cpIndex = getMemberCPIndex(constantPool, cClassName, "field3", "I");
		Assert.assertTrue(-1 != cpIndex, "couldn't find field3 fref");
		annotationFound = containsRuntimeAnnotation(C.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertTrue(annotationFound, "did not detect @Stable annotation");
	}

	@Test
	public void test_static_field_in_external_class() throws Exception {
		boolean annotationFound = false;
		int cpIndex;
		final String stableAnnotation = "Ljdk/internal/vm/annotation/Stable;";

		/* resolve fields */
		C.sMethod1();

		ConstantPool constantPool = (ConstantPool)getConstantPool.invoke(javaLangAccess, C.class);
		final String cClassName = "org/openj9/test/annotation/C";
		final String bClassName = "org/openj9/test/annotation/B";

		cpIndex = getMemberCPIndex(constantPool, bClassName, "sfield1", "I");
		Assert.assertTrue(-1 != cpIndex, "couldn't find sfield1 fref");
		annotationFound = containsRuntimeAnnotation(C.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertTrue(annotationFound, "did not detect @Stable annotation");

		cpIndex = getMemberCPIndex(constantPool, bClassName, "sfield2", "I");
		Assert.assertTrue(-1 != cpIndex, "couldn't find sfield2 fref");
		annotationFound = containsRuntimeAnnotation(C.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertTrue(annotationFound, "did not detect @Stable annotation");

		cpIndex = getMemberCPIndex(constantPool, cClassName, "sfield3", "I");
		Assert.assertTrue(-1 != cpIndex, "couldn't find sfield3 fref");
		annotationFound = containsRuntimeAnnotation(C.class, cpIndex, stableAnnotation, true, 0);
		Assert.assertTrue(annotationFound, "did not detect @Stable annotation");
	}

	private int getMemberCPIndex(ConstantPool constantPool, String className, String memberName, String memberType) {
		int cpIndex = -1;
		int size = constantPool.getSize();
		Assert.assertTrue(size > 1, "error with constantPool");

		for (int i = size - 1; i > 0; i--) {
			try {
				/* Returns 3-element array of class name, member name and type */
				String [] cpMemberInfo = constantPool.getMemberRefInfoAt(i);
				if (className.equals(cpMemberInfo[0])
					&& memberName.equals(cpMemberInfo[1])
					&& memberType.equals(cpMemberInfo[2])
				) {
					cpIndex = i;
					break;
				}
			} catch (Throwable ignored) {
				/* Ignore errors if the constant pool entry doesn't exist */
			}
		}

		return cpIndex;
	}
}

class A {
	A() {}

	@Stable
	int field1;

	@Stable
	static int sfield1;
}

class B extends A {
	B() {}

	@Stable
	int field2;

	@Stable
	static int sfield2;
}

class C {
	C() {}

	@Stable
	int field3;

	@Stable
	static int sfield3;

	void method1(B b) {
		int field1 = b.field1;
		int field2 = b.field2;
		int field3 = this.field3;
	}

	static void sMethod1() {
		int field1 = B.sfield1;
		int field2 = B.sfield2;
		int field3 = C.sfield3;
	}
}

@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.FIELD)
@interface MyFieldAnnotation {}

@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.FIELD)
@interface MyFieldAnnotation2 {}

@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.FIELD)
@interface MyFieldAnnotation3 {}

@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.FIELD)
@interface MyFieldAnnotation4 {}
