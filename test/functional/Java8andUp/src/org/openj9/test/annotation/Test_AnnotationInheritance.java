package org.openj9.test.annotation;
/*
 * Copyright IBM Corp. and others 2026
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
 * Assisted-by: IBM Bob
 */

import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

import org.objectweb.asm.AnnotationVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;

import org.testng.Assert;
import org.testng.annotations.Test;

@Test(groups = { "level.sanity" })
public class Test_AnnotationInheritance {
	private static final String PREFIX = "org/openj9/test/annotation/generated/";

	private static class TestClassLoader extends ClassLoader {
		Class<?> define(byte[] bytes) {
			return defineClass(null, bytes, 0, bytes.length);
		}
	}

	/* Generate an annotation containing an enum constant that
	 * is unavailable on the running JDK.
	 */
	private static byte[] createAnnotation(String name, boolean inherited) {
		ClassWriter writer = new ClassWriter(0);

		writer.visit(
				Opcodes.V1_8,
				Opcodes.ACC_PUBLIC | Opcodes.ACC_INTERFACE | Opcodes.ACC_ABSTRACT | Opcodes.ACC_ANNOTATION,
				name, null, "java/lang/Object",
				new String[] { "java/lang/annotation/Annotation" });

		AnnotationVisitor retention = writer.visitAnnotation("Ljava/lang/annotation/Retention;", true);
		retention.visitEnum("value", "Ljava/lang/annotation/RetentionPolicy;", "RUNTIME");
		retention.visitEnd();

		AnnotationVisitor target = writer.visitAnnotation("Ljava/lang/annotation/Target;", true);
		AnnotationVisitor values = target.visitArray("value");

		values.visitEnum(null, "Ljava/lang/annotation/ElementType;", "TYPE");
		values.visitEnum(null, "Ljava/lang/annotation/ElementType;", "UNDEFINED_FUTURE_ELEMENT");

		values.visitEnd();
		target.visitEnd();

		if (inherited) {
			writer.visitAnnotation("Ljava/lang/annotation/Inherited;", true).visitEnd();
		}

		writer.visitEnd();
		return writer.toByteArray();
	}

	private static byte[] createClass(String name, String superclass, String annotation) {
		ClassWriter writer = new ClassWriter(0);

		writer.visit(
				Opcodes.V1_8,
				Opcodes.ACC_PUBLIC | Opcodes.ACC_SUPER,
				name, null, superclass, null);

		if (annotation != null) {
			writer.visitAnnotation("L" + annotation + ";", true).visitEnd();
		}

		writer.visitEnd();
		return writer.toByteArray();
	}

	private void testUnknownTarget(boolean inherited) {
		String annotationName = PREFIX + "FutureAnnotation";
		String parentName = PREFIX + "Parent";
		String childName = PREFIX + "Child";
		TestClassLoader loader = new TestClassLoader();
		Class<? extends Annotation> annotationClass = loader.define(createAnnotation(annotationName, inherited)).asSubclass(Annotation.class);
		Class<?> parent = loader.define(createClass(parentName, "java/lang/Object", annotationName));
		Class<?> child = loader.define(createClass(childName, parentName, null));

		Assert.assertNotNull(parent.getDeclaredAnnotation(annotationClass));
		Assert.assertNull(child.getDeclaredAnnotation(annotationClass));

		Annotation result = child.getAnnotation(annotationClass);

		if (inherited) {
			Assert.assertNotNull(result);
			Assert.assertEquals(child.getAnnotations().length, 1);
		} else {
			Assert.assertNull(result);
			Assert.assertEquals(child.getAnnotations().length, 0);
		}
	}

	@Test
	public void testNonInheritedUnknownTarget() {
		testUnknownTarget(false);
	}

	@Test
	public void testInheritedUnknownTarget() {
		testUnknownTarget(true);
	}

	@Retention(RetentionPolicy.RUNTIME)
	@Target(ElementType.TYPE)
	@Inherited
	public @interface InheritedMarker {
		String value();
	}

	@Retention(RetentionPolicy.RUNTIME)
	@Target(ElementType.TYPE)
	public @interface NonInheritedMarker {
	}

	@InheritedMarker("parent")
	@NonInheritedMarker
	static class Parent {
	}

	static class Child extends Parent {
	}

	@InheritedMarker("child")
	static class OverridingChild extends Parent {
	}

	@Test
	public void testStandardInheritance() {
		Assert.assertEquals(Child.class.getAnnotation(InheritedMarker.class).value(), "parent");
		Assert.assertNull(Child.class.getDeclaredAnnotation(InheritedMarker.class));
		Assert.assertNull(Child.class.getAnnotation(NonInheritedMarker.class));
		Assert.assertEquals(Child.class.getAnnotationsByType(InheritedMarker.class).length, 1);
		Assert.assertEquals(Child.class.getAnnotationsByType(NonInheritedMarker.class).length, 0);
		Assert.assertEquals(OverridingChild.class.getAnnotation(InheritedMarker.class).value(), "child");
	}
}
