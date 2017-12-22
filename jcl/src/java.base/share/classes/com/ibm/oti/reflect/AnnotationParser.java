/*[INCLUDE-IF Sidecar16]*/
package com.ibm.oti.reflect;

/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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

import java.lang.annotation.Annotation;
import java.nio.ByteBuffer;
/*[IF Sidecar19-SE]
import jdk.internal.reflect.ConstantPool;
/*[ELSE]*/
import sun.reflect.ConstantPool;
/*[ENDIF]*/

public class AnnotationParser {

	public static Annotation[] parseAnnotations(java.lang.reflect.Field field) {
		return parseAnnotations(getAnnotationsData(field), field.getDeclaringClass());
	}

	public static Annotation[] parseAnnotations(java.lang.reflect.Constructor constructor) {
		return parseAnnotations(getAnnotationsData(constructor), constructor.getDeclaringClass());
	}

	public static Annotation[] parseAnnotations(java.lang.reflect.Method method) {
		return parseAnnotations(getAnnotationsData(method), method.getDeclaringClass());
	}

	public static Annotation[][] parseParameterAnnotations(java.lang.reflect.Constructor constructor) {
		return parseParameterAnnotations(getParameterAnnotationsData(constructor), constructor.getDeclaringClass(), constructor.getParameterTypes().length);
	}

	public static Annotation[][] parseParameterAnnotations(java.lang.reflect.Method method) {
		return parseParameterAnnotations(getParameterAnnotationsData(method), method.getDeclaringClass(), method.getParameterTypes().length);
	}
	
	/**
	 * @param clazz class for which annotations are to be retrieved
	 * @return annotation attribute bytes, or null if clazz is null.
	 */
	public static  byte[] getAnnotationsData(java.lang.Class clazz) {
		byte[] result = null; 
		if (null != clazz) {
			result = getAnnotationsDataImpl(clazz);
		}
		return result;
	};
	
	public static Object parseDefaultValue(java.lang.reflect.Method method) {
		byte[] elementValueData = getDefaultValueData(method);
		if (elementValueData == null) return null;
		ByteBuffer buf = ByteBuffer.wrap(elementValueData);
		Class clazz = method.getDeclaringClass();
		
		/* The AnnotationParser boxes primitive return types */
		Class returnType = method.getReturnType();
		if (returnType.isPrimitive()) {
			if (returnType == Boolean.TYPE) {
				returnType = Boolean.class;
			} else if (returnType == Character.TYPE) {
				returnType = Character.class;
			} else if (returnType == Byte.TYPE) {
				returnType = Byte.class;
			} else if (returnType == Short.TYPE) {
				returnType = Short.class;
			} else if (returnType == Integer.TYPE) {
				returnType = Integer.class;
			} else if (returnType == Long.TYPE) {
				returnType = Long.class;
			} else if (returnType == Float.TYPE) {
				returnType = Float.class;
			} else if (returnType == Double.TYPE) {
				returnType = Double.class;
			}
		}
		return sun.reflect.annotation.AnnotationParser.parseMemberValue(returnType, buf, getConstantPool(clazz), clazz);
	}

	public static Annotation[] parseAnnotations(byte[] annotationsData, Class clazz) {
		return sun.reflect.annotation.AnnotationParser.toArray(
				sun.reflect.annotation.AnnotationParser.parseAnnotations(
						annotationsData,
						getConstantPool(clazz),
						clazz));
	}

	private static native byte[] getAnnotationsData(java.lang.reflect.Field field);
	private static native byte[] getAnnotationsData(java.lang.reflect.Constructor constructor);
	private static native byte[] getAnnotationsData(java.lang.reflect.Method method);
	private static native byte[] getParameterAnnotationsData(java.lang.reflect.Constructor constructor);
	private static native byte[] getParameterAnnotationsData(java.lang.reflect.Method method);
	private static native byte[] getDefaultValueData(java.lang.reflect.Method method);
	static native ConstantPool getConstantPool(Class clazz);
	private static native byte[] getAnnotationsDataImpl(java.lang.Class clazz);

	private static Annotation[][] parseParameterAnnotations(byte[] annotationsData, Class<?> declaringClass, int parametersCount) {
		if (annotationsData == null) {
			Annotation[][] annotations = new Annotation[parametersCount][];
			for (int i = 0; i < parametersCount; i++) {
				annotations[i] = new Annotation[0];
			}
			return annotations;
		}
		return sun.reflect.annotation.AnnotationParser.parseParameterAnnotations(annotationsData, getConstantPool(declaringClass), declaringClass);
	}
}
