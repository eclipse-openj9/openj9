/*[INCLUDE-IF Sidecar18-SE]*/
package com.ibm.oti.reflect;

import java.lang.reflect.AnnotatedType;
import java.lang.reflect.Constructor;

/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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
/**
 * Retrieve RuntimeVisibleType Annotation attributes (JVM spec, section 4.7.20) from the ROM class and parse them into Java objects
 *
 */

public class TypeAnnotationParser {
	private static final byte[] EMPTY_TYPE_ANNOTATIONS_ATTRIBUTE = new byte[] {0, 0}; /* just num_annotations=0 */
	
	/**
	 * @param jlrConstructor constructor for which annotations are to be retrieved
	 * @return annotation attribute bytes, not including the attribute_name_index and attribute_length fields, or null if jlrConstructor is null.
	 */
	public static  byte[] getTypeAnnotationsData(java.lang.reflect.Constructor jlrConstructor) {
		byte[] result = null;
		if (null != jlrConstructor) {
			result = getTypeAnnotationsDataImpl(jlrConstructor);
		}
		return result;
	};
	/**
	 * @param jlrMethod method for which annotations are to be retrieved
	 * @return annotation attribute bytes, not including the attribute_name_index and attribute_length fields, or null if jlrMethod is null.
	 */
	public static  byte[] getTypeAnnotationsData(java.lang.reflect.Method jlrMethod) {
		byte[] result = null;
		if (null != jlrMethod) {
			result =  getTypeAnnotationsDataImpl(jlrMethod);
		}
		return result;
	};
	
	/**
	 * @param jlrField field for which annotations are to be retrieved
	 * @return annotation attribute bytes, not including the attribute_name_index and attribute_length fields, or null if jlrField is null.
	 */
	public static  byte[] getTypeAnnotationsData(java.lang.reflect.Field jlrField) {
		byte[] result = null;
		if (null != jlrField) {
			result =  getTypeAnnotationsDataImpl(jlrField);
		}
		return result;
	};
	
	/**
	 * @param clazz class for which annotations are to be retrieved
	 * @return annotation attribute bytes, not including the attribute_name_index and attribute_length fields, or null if clazz is null.
	 */
	public static  byte[] getTypeAnnotationsData(java.lang.Class clazz) {
		byte[] result = null;
		if (null != clazz) {
			result =  getTypeAnnotationsDataImpl(clazz);
		}
		return result;
	};
	private static native byte[] getTypeAnnotationsDataImpl(java.lang.reflect.Field jlrField);
	private static native byte[] getTypeAnnotationsDataImpl(java.lang.Class clazz);
	private static native byte[] getTypeAnnotationsDataImpl(java.lang.reflect.Method jlrMethod);
	private static native byte[] getTypeAnnotationsDataImpl(Constructor jlrConstructor);

	private static byte[] getAttributeData(Class clazz) {
		byte[] attr = getTypeAnnotationsData(clazz);
		if (null == attr) {
			attr = EMPTY_TYPE_ANNOTATIONS_ATTRIBUTE;
		}
		return attr;
	}
	
	/**
	 * @param clazz class  for which annotated interfaces are to be retrieved
	 * @return array (possibly empty) of AnnotatedType objects for the interfaces.
	 * @see java.lang.Class getAnnotatedInterfaces
	 */
	public  static AnnotatedType[] buildAnnotatedInterfaces(Class clazz) {
		byte[] attr = getAttributeData(clazz);
		AnnotatedType[] annotatedInterfaces = sun.reflect.annotation.TypeAnnotationParser.buildAnnotatedInterfaces(attr, AnnotationParser.getConstantPool(clazz), clazz);
		return annotatedInterfaces;
	}
	/**
	 * @param clazz class  for which annotated superclass are to be retrieved
	 * @return (possibly empty) AnnotatedType object for the superclass.
	 * @see java.lang.Class getAnnotatedSuperclass
	 */
	public  static AnnotatedType buildAnnotatedSupertype(Class clazz) {
		byte[] attr = getAttributeData(clazz);
		AnnotatedType annotatedSuperclass = sun.reflect.annotation.TypeAnnotationParser.buildAnnotatedSuperclass(attr, AnnotationParser.getConstantPool(clazz), clazz);
		return annotatedSuperclass;
	}
}
