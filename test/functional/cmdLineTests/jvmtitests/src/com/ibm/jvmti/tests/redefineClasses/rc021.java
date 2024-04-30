/*
 * Copyright IBM Corp. and others 2021
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
package com.ibm.jvmti.tests.redefineClasses;

import com.ibm.jvmti.tests.util.Util;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedType;
import java.util.Arrays;

public class rc021 {
	public static native boolean redefineClass(Class name, int classBytesSize,
			byte[] classBytes);

	final static int ITERATIONS = 100;
	static boolean result;

	static byte classBytesO_1[];
	static byte classBytesR_1[];
	static byte classBytesO_2[];
	static byte classBytesR_2[];

	static {
		classBytesO_1 = Util.getClassBytes(rc021_testAnnotationCache_O.class);
		if (classBytesO_1 == null) {
			throw new RuntimeException("Class bytes null");
		}
		classBytesR_1 = Util.loadRedefinedClassBytesWithOriginalClassName(rc021_testAnnotationCache_O.class, rc021_testAnnotationCache_R.class);
		if (classBytesR_1 == null) {
			throw new RuntimeException("Class bytes null");
		}

		classBytesO_2 = Util.getClassBytes(rc021_testAnnotatedType_O.class);
		if (classBytesO_2 == null) {
			throw new RuntimeException("Class bytes null");
		}
		classBytesR_2 = Util.loadRedefinedClassBytesWithOriginalClassName(rc021_testAnnotatedType_O.class, rc021_testAnnotatedType_R.class);
		if (classBytesR_2 == null) {
			throw new RuntimeException("Class bytes null");
		}
	}

	public boolean testAnnotationCacheAfterRedefine() {
		result = true;
		try {
			final class AnnotationFetch implements Runnable {
				@Override
				public void run() {
					try {
						Class original = rc021_testAnnotationCache_O.class;

						Annotation[] ann = original.getDeclaredAnnotations();
						if (ann.length == 0) {
							System.out.println("No annotations");
						} else {
							System.out.println("Annotations:");
							System.out.println(Arrays.toString(ann));
						}
					} catch (Throwable t) {
						System.out.println("Exception during annotation fetch:");
						t.printStackTrace(System.out);
						result = false;
					}
				}
			};

			final class Redef implements Runnable {
				@Override
				public void run() {
					boolean redefined = redefineClass(rc021_testAnnotationCache_O.class, classBytesR_1.length, classBytesR_1);
					if (!redefined) {
						System.out.println("Error during redefine from original to redefined");
						result = false;
					}
					redefined = redefineClass(rc021_testAnnotationCache_O.class, classBytesO_1.length, classBytesO_1);
					if (!redefined) {
						System.out.println("Error during redefine from redefined to original");
						result = false;
					}
				}
			};

			ExecutorService executor = Executors.newCachedThreadPool();
			for (int i = 0; i < ITERATIONS; i++) {
				executor.submit(new AnnotationFetch());
				executor.submit(new Redef());
			}

			/* Execute the tasks before shutting down */
			executor.shutdown();
			executor.awaitTermination(1, TimeUnit.MINUTES);
			return result;
		} catch (Throwable t) {
			System.out.println("Exception during test:");
			t.printStackTrace(System.out);
			return false;
		}
	}

	public String helpAnnotationCacheAfterRedefine() {
		return "Tests that annotation cache data is consistent with class's constant pool";
	}

	public boolean testAnnotatedTypes() {
		result = true;
		try {
			final class AnnotationFetch implements Runnable {
				@Override
				public void run() {
					try {
						Class original = rc021_testAnnotatedType_O.class;

						AnnotatedType[] at = original.getAnnotatedInterfaces();
						if (at.length == 0) {
							System.out.println("No interface annotations");
						} else {
							System.out.println("Interface annotations:");
							for (AnnotatedType a : at) {
								System.out.println(Arrays.toString(a.getAnnotations()));
							}
						}
						Annotation[] ann2 = original.getAnnotatedSuperclass().getAnnotations();
						if (ann2.length == 0) {
							System.out.println("No superclass annotations");
						} else {
							System.out.println("Superclass annotations:");
							System.out.println(Arrays.toString(ann2));
						}
					} catch (Throwable t) {
						System.out.println("Exception during annotation fetch:");
						t.printStackTrace(System.out);
						result = false;
					}
				}
			};

			final class Redef implements Runnable {
				@Override
				public void run() {
					boolean redefined = redefineClass(rc021_testAnnotatedType_O.class, classBytesR_2.length, classBytesR_2);
					if (!redefined) {
						System.out.println("Error during redefine from original to redefined");
						result = false;
					}
					redefined = redefineClass(rc021_testAnnotatedType_O.class, classBytesO_2.length, classBytesO_2);
					if (!redefined) {
						System.out.println("Error during redefine from redefined to original");
						result = false;
					}
				}
			};

			ExecutorService executor = Executors.newCachedThreadPool();
			for (int i = 0; i < ITERATIONS; i++) {
				executor.submit(new AnnotationFetch());
				executor.submit(new Redef());
			}

			/* Execute the tasks before shutting down */
			executor.shutdown();
			executor.awaitTermination(1, TimeUnit.MINUTES);
			return result;
		} catch (Throwable t) {
			System.out.println("Exception during test:");
			t.printStackTrace(System.out);
			return false;
		}
	}

	public String helpAnnotatedTypes() {
		return "Tests that annotated types data for implemented interfaces and extended superclass is consistent "
				+ "with the class's constant pool";
	}
}
