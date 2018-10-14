package org.openj9.test.annotation;

/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Inherited;

@Retention(RetentionPolicy.RUNTIME)
@Inherited()
public @interface ValueAnnotation {
	String value() default "ValueAnnotationDefault";

	boolean booleanValue() default true;

	char charValue() default 'q';

	byte byteValue() default 123;

	short shortValue() default 12345;

	int intValue() default 1234567890;

	float floatValue() default 12345.6789f;

	double doubleValue() default 12345.6789;

	long longValue() default 1234567890123456789l;

	String stringValue() default "stringValue";

	EnumClass enumValue() default EnumClass.Value1;

	Class classValue() default EnumClass.class;

	ClassAnnotation annotationValue() default @ClassAnnotation();

	boolean[] booleanArrayValue() default { true, false };

	char[] charArrayValue() default { 'q', 'm' };

	byte[] byteArrayValue() default { 123, -123 };

	short[] shortArrayValue() default { 12345, -12345 };

	int[] intArrayValue() default { 1234567890, -1234567890 };

	float[] floatArrayValue() default { 12345.6789f, -12345.6789f };

	double[] doubleArrayValue() default { 12345.6789, -12345.6789 };

	long[] longArrayValue() default { 1234567890123456789l, -1234567890123456789l };

	String[] stringArrayValue() default { "stringValue", "valueString" };

	EnumClass[] enumArrayValue() default { EnumClass.Value1, EnumClass.Value2 };

	Class[] classArrayValue() default { AnnotationTestClass.class, EnumClass.class };

	ClassAnnotation[] annotationArrayValue() default { @ClassAnnotation(), @ClassAnnotation() };
}
