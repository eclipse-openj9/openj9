/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package org.openj9.test.javaagenttest;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.BeforeMethod;

import java.io.IOException;
import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Arrays;

import static java.lang.annotation.RetentionPolicy.RUNTIME;

import jdk.internal.org.objectweb.asm.AnnotationVisitor;
import jdk.internal.org.objectweb.asm.ClassReader;
import jdk.internal.org.objectweb.asm.ClassVisitor;
import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.FieldVisitor;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Type;
import static jdk.internal.org.objectweb.asm.Opcodes.ASM7;

public class Cmvc196982 {
	private static final Logger logger = Logger.getLogger(Cmvc196982.class);

	@Retention(value=RUNTIME)
	public @interface MyAnnotation1 {}

	@Retention(value=RUNTIME)
	public @interface MyAnnotation2 {}

	public static class C {
		@MyAnnotation1
		public C() {}
		@MyAnnotation1
		public void m() {}
		@MyAnnotation1
		public int f;
	}

	private Field annotationCacheField;
	private Method getMethodAnnotationBytes;
	private Method getConstructorAnnotationBytes;
	private Field reflectCacheField;
	private Method reflectFieldRoot;
	private Method reflectMethodRoot;
	private Method reflectConstructorRoot;

	@BeforeMethod(groups = { "level.sanity" })
	protected void setUp() throws Exception {
		logger.info("Cmvc196982 is setting up... ");

		/* Java 12 disallows direct field reflection on java.lang.reflect.Method
		 * (refer https://github.com/eclipse/openj9/issues/4658#issuecomment-461913909 for details), 
		 * this is a workaround via reflection of package access methods.
		 */
		getMethodAnnotationBytes = Method.class.getDeclaredMethod("getAnnotationBytes");
		getMethodAnnotationBytes.setAccessible(true);
		/* java.lang.reflect.Field doesn't have method getAnnotationBytes()*/
		getConstructorAnnotationBytes = Constructor.class.getDeclaredMethod("getAnnotationBytes");
		getConstructorAnnotationBytes.setAccessible(true);

		annotationCacheField = Class.class.getDeclaredField("annotationCache");
		annotationCacheField.setAccessible(true);

		reflectCacheField = Class.class.getDeclaredField("reflectCache");
		reflectCacheField.setAccessible(true);

		reflectFieldRoot = Field.class.getDeclaredMethod("getRoot");
		reflectFieldRoot.setAccessible(true);
		
		reflectMethodRoot = Method.class.getDeclaredMethod("getRoot");
		reflectMethodRoot.setAccessible(true);
		
		reflectConstructorRoot = Constructor.class.getDeclaredMethod("getRoot");
		reflectConstructorRoot.setAccessible(true);
	}

	/**
	 * Check that the reflect caches for a ClassLoader are flushed when a class it has loaded is redefined.
	 * This is a white-box test, using reflection to inspect private fields of ClassLoader and Method, Field
	 * & Constructor. A target class, containing a field, a method and a default constructor, each annotated
	 * with MyAnnotation1, is redefined to add MyAnnotation2 to each element.
	 * The reflect caches should be non-null before the call to redefine and should be null afterwards. The
	 * annotation attribute bytes from reflect objects obtained before the call to redefine should remain the
	 * same afterwards. Refetched reflect objects should contain updated annotation attribute bytes (indicating
	 * that they have been populated by the VM rather than returned from the reflect cache).
	 *
	 * @throws ReflectiveOperationException
	 * @throws SecurityException
	 * @throws IOException
	 */
	@Test(groups = { "level.sanity", "level.extended" })
	public void testAddingAnnotationsFlushesReflectCache() throws ReflectiveOperationException, SecurityException, IOException {
		logger.info("Cmvc196982 is testing testAddingAnnotationsFlushesReflectCache()... ");

		Class<?> clazz = C.class;
		Method m = clazz.getMethod("m");
		Field f = clazz.getField("f");
		Constructor<?> c = clazz.getConstructor();
		clazz.getAnnotations();

		/* Reflect caches should be non-null before redefine, due to the reflect calls above */
		AssertJUnit.assertNotNull("reflect cache is null before redefine", reflectCacheField.get(clazz) );
		AssertJUnit.assertNotNull("annotation cache is null before redefine", annotationCacheField.get(clazz) );

		/* Equivalent objects returned from reflect cache should share "root" field */
		checkRoots();

		/* Fetch annotations byte arrays before redefining */
		byte[] methodBeforeRedefine = (byte[]) getMethodAnnotationBytes.invoke(m);
		byte[] constructorBeforeRedefine = (byte[]) getConstructorAnnotationBytes.invoke(c);

		/* Parse original annotations byte arrays */
		Annotation[] methodOriginalAnnotations = m.getAnnotations();
		Annotation[] fieldOriginalAnnotations = f.getAnnotations();
		Annotation[] constructorOriginalAnnotations = c.getAnnotations();

		AgentMain.redefineClass(clazz, addAnnotation(clazz, MyAnnotation2.class));

		/* Reflect caches should be nulled by the redefine above */
		AssertJUnit.assertNull("reflect cache is not null after redefine", reflectCacheField.get(clazz) );
		AssertJUnit.assertNull("annotation cache is not null after redefine", annotationCacheField.get(clazz) );

		/* Cache should be repopulated, and equivalent objects returned from reflect cache should share "root" field */
		checkRoots();

		/* Fetch annotations byte arrays after redefining */
		byte[] methodAfterRedefine = (byte[]) getMethodAnnotationBytes.invoke(m);
		byte[] constructorAfterRedefine = (byte[]) getConstructorAnnotationBytes.invoke(c);

		/* Refetch reflect objects */
		m = clazz.getMethod("m");
		f = clazz.getField("f");
		c = clazz.getConstructor();

		/* Fetch annotations byte arrays from refetched reflect objects */
		byte[] methodAfterRefetch = (byte[]) getMethodAnnotationBytes.invoke(m);
		byte[] constructorAfterRefetch = (byte[]) getConstructorAnnotationBytes.invoke(c);

		checkAnnotations(methodBeforeRedefine, methodAfterRedefine, methodAfterRefetch, methodOriginalAnnotations, m.getAnnotations());
		checkAnnotations(constructorBeforeRedefine, constructorAfterRedefine, constructorAfterRefetch, constructorOriginalAnnotations, c.getAnnotations());
	}

	/* Reflect objects returned from the cache should share their "root" field */
	private void checkRoots() throws ReflectiveOperationException, SecurityException {
		Class<?> clazz = C.class;

		Field f1 = clazz.getField("f");
		Field f2 = clazz.getField("f");
		AssertJUnit.assertNotNull("Field instances not cached ", reflectFieldRoot.invoke(f1));
		AssertJUnit.assertEquals(reflectFieldRoot.invoke(f2), reflectFieldRoot.invoke(f1));


		Method m1 = clazz.getMethod("m");
		Method m2 = clazz.getMethod("m");
		AssertJUnit.assertNotNull("Field instances not cached ", reflectMethodRoot.invoke(m1));
		AssertJUnit.assertEquals(reflectMethodRoot.invoke(m2), reflectMethodRoot.invoke(m1));

		Constructor<?> c1 = clazz.getConstructor();
		Constructor<?> c2 = clazz.getConstructor();
		AssertJUnit.assertNotNull("Field instances not cached ", reflectConstructorRoot.invoke(c1));
		AssertJUnit.assertEquals(reflectConstructorRoot.invoke(c2), reflectConstructorRoot.invoke(c1));
	}

	private void checkAnnotations(byte[] beforeRedefine, byte[] afterRedefine, byte[] afterRefetch, Annotation[] originalAnnotations, Annotation[] refetchedAnnotations) {
		AssertJUnit.assertArrayEquals("annotations byte array changed during redefine", beforeRedefine, afterRedefine);
		AssertJUnit.assertFalse("annotations byte array didn't change after refetching Method", Arrays.equals(afterRedefine, afterRefetch));
		AssertJUnit.assertFalse("annotations weren't updated after refetching Method", Arrays.equals(originalAnnotations, refetchedAnnotations));
		AssertJUnit.assertEquals("too many annotations on original method", originalAnnotations.length, 1 );
	}

	/*
	 * Transform classToTransform by adding the given annotation to all already-annotated methods, fields and constructors in the class.
	 * Return the modified class file bytes.
	 */
	private byte[] addAnnotation(Class<?> classToTransform, final Class<?> annotation) throws IOException {
		ClassWriter writer = new ClassWriter(0);
		new ClassReader(Type.getInternalName(classToTransform)).accept(new ClassVisitor(ASM7, writer) {
			@Override
			public MethodVisitor visitMethod(int access, String name, String desc, String signature, String[] exceptions) {
				return new MethodVisitor(ASM7, cv.visitMethod(access, name, desc, signature, exceptions)) {
					@Override
					public AnnotationVisitor visitAnnotation(String desc, boolean visible) {
						mv.visitAnnotation(Type.getDescriptor(annotation), true).visitEnd();
						return mv.visitAnnotation(desc, visible);
					}
				};
			}
			@Override
			public FieldVisitor visitField(int access, String name, String desc, String signature, Object value) {
				return new FieldVisitor(ASM7, cv.visitField(access, name, desc, signature, value)) {
					@Override
					public AnnotationVisitor visitAnnotation(String desc, boolean visible) {
						fv.visitAnnotation(Type.getDescriptor(annotation), true).visitEnd();
						return fv.visitAnnotation(desc, visible);
					}
				};
			}
		}, 0);
		return writer.toByteArray();
	}
}
