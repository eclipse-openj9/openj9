package org.openj9.test.annotation;
/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.annotation.Annotation;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Vector;

import org.openj9.test.annotation.defaults.AnnotationTestClassDefaults;
import org.openj9.test.annotationPackage.Test_PackageAnnotation2;

@Test(groups = { "level.sanity" })
public class Test_Annotation {
	private Class<?> testClass = AnnotationTestClass.class;
	private Class<?> testClassDefaults = AnnotationTestClassDefaults.class;
	private final String packageAnnotation = "PackageAnnotation";
	private final String ClassAnnotation = "ClassAnnotation";
	private final String ConstructorAnnotation = "ConstructorAnnotation";
	private final String MethodAnnotation = "MethodAnnotation";
	private final String FieldAnnotation = "FieldAnnotation";
	private final boolean booleanValue = true;
	private final boolean booleanValueDefault = true;
	private final boolean[] booleanArrayValue = new boolean[] { false, true };
	private final boolean[] booleanArrayValueDefault = new boolean[] { true, false };
	private final char charValue = 'm';
	private final char charValueDefault = 'q';
	private final char[] charArrayValue = new char[] { 'm', 'q' };
	private final char[] charArrayValueDefault = new char[] { 'q', 'm' };
	private final byte byteValue = -123;
	private final byte byteValueDefault = 123;
	private final byte[] byteArrayValue = new byte[] { -123, 123 };
	private final byte[] byteArrayValueDefault = new byte[] { 123, -123 };
	private final short shortValue = -12345;
	private final short shortValueDefault = 12345;
	private final short[] shortArrayValue = new short[] { -12345, 12345 };
	private final short[] shortArrayValueDefault = new short[] { 12345, -12345 };
	private final int intValue = -1234567890;
	private final int intValueDefault = 1234567890;
	private final int[] intArrayValue = new int[] { -1234567890, 1234567890 };
	private final int[] intArrayValueDefault = new int[] { 1234567890, -1234567890 };
	private final float floatValue = -12345.6789f;
	private final float floatValueDefault = 12345.6789f;
	private final float[] floatArrayValue = new float[] { -12345.6789f, 12345.6789f };
	private final float[] floatArrayValueDefault = new float[] { 12345.6789f, -12345.6789f };
	private final double doubleValue = -12345.6789;
	private final double doubleValueDefault = 12345.6789;
	private final double[] doubleArrayValue = new double[] { -12345.6789, 12345.6789 };
	private final double[] doubleArrayValueDefault = new double[] { 12345.6789, -12345.6789 };
	private final long longValue = -1234567890123456789l;
	private final long longValueDefault = 1234567890123456789l;
	private final long[] longArrayValue = new long[] { -1234567890123456789l, 1234567890123456789l };
	private final long[] longArrayValueDefault = new long[] { 1234567890123456789l, -1234567890123456789l };
	private final String stringValue = "valueString";
	private final String stringValueDefault = "stringValue";
	private final String[] stringArrayValue = new String[] { "valueString", "stringValue" };
	private final String[] stringArrayValueDefault = new String[] { "stringValue", "valueString" };
	private final Enum enumValue = EnumClass.Value3;
	private final Enum enumValueDefault = EnumClass.Value1;
	private final Enum[] enumArrayValue = new Enum[] { EnumClass.Value2, EnumClass.Value1 };
	private final Enum[] enumArrayValueDefault = new Enum[] { EnumClass.Value1, EnumClass.Value2 };
	private final Class classValue = AnnotationTestClass.class;
	private final Class classValueDefault = EnumClass.class;
	private final Class[] classArrayValue = new Class[] { EnumClass.class, AnnotationTestClass.class };
	private final Class[] classArrayValueDefault = new Class[] { AnnotationTestClass.class, EnumClass.class };
	private final String annotationValue = "ClassAnnotation";
	private final String annotationValueDefault = "ClassAnnotationDefault";
	private final String[] annotationArrayValue = new String[] { "ClassAnnotation1", "ClassAnnotation2" };
	private final String[] annotationArrayValueDefault = new String[] {
			"ClassAnnotationDefault",
			"ClassAnnotationDefault" };
	private final String[][] parameterValueJava8 = new String[][] {
			{ "@org.openj9.test.annotation.ParameterAnnotation(value=ParameterAnnotation)", "@java.lang.Deprecated()" },
			{ "@org.openj9.test.annotation.ParameterAnnotation(value=ParameterAnnotation)" } };
	private final String[][] parameterValueJava9 = new String[][] {
				{ "@org.openj9.test.annotation.ParameterAnnotation(value=\"ParameterAnnotation\")", "@java.lang.Deprecated(forRemoval=false, since=\"\")" },
				{ "@org.openj9.test.annotation.ParameterAnnotation(value=\"ParameterAnnotation\")" } };
	private final String[][] noParameterValue = new String[][] { {} };
	private final String[][] emptyParameterListValue = new String[][] {};
	private final String[] noAnnotations = new String[] {};
	private final String[][] parameterValueDefault = new String[][] {
			{
					"@org.openj9.test.annotation.ParameterAnnotation(value=ParameterAnnotationDefault)",
					"@java.lang.Deprecated()" },
			{ "@org.openj9.test.annotation.ParameterAnnotation(value=ParameterAnnotationDefault)" } };
	static Logger logger = Logger.getLogger(Test_Annotation.class);

	public Test_Annotation() {
	}

	@Test
	@TestAnnotation()
	public void test_package_annotations() throws Exception {
		Package pack = testClass.getPackage();
		PackageAnnotation annotation = (PackageAnnotation)pack.getAnnotation(PackageAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("PackageAnnotation not found on " + pack.toString());
		}
		myAssert(packageAnnotation, annotation.value());
	}

	@Test
	@TestAnnotation()
	public void test_package_boolean_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(booleanValue, annotation.booleanValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_char_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(charValue, annotation.charValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_byte_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(byteValue, annotation.byteValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_short_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(shortValue, annotation.shortValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_int_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(intValue, annotation.intValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_float_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(floatValue, annotation.floatValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_double_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(doubleValue, annotation.doubleValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_long_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(longValue, annotation.longValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_String_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(stringValue, annotation.stringValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_Enum_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(enumValue, annotation.enumValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_Class_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(classValue, annotation.classValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_Annotation_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(annotationValue, annotation.annotationValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_boolean_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(booleanArrayValue, annotation.booleanArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_char_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(charArrayValue, annotation.charArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_byte_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(byteArrayValue, annotation.byteArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_short_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(shortArrayValue, annotation.shortArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_int_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(intArrayValue, annotation.intArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_float_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(floatArrayValue, annotation.floatArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_double_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(doubleArrayValue, annotation.doubleArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_long_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(longArrayValue, annotation.longArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_String_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(stringArrayValue, annotation.stringArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_Enum_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(enumArrayValue, annotation.enumArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_Class_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(classArrayValue, annotation.classArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_package_Annotation_array_annotation() throws Exception {
		Package pack = testClass.getPackage();
		ValueAnnotation annotation = (ValueAnnotation)pack.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + pack.toString());
		}
		myAssert(annotationArrayValue, annotation.annotationArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_annotations() throws Exception {
		ClassAnnotation annotation = (ClassAnnotation)testClass.getAnnotation(ClassAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ClassAnnotation not found on " + testClass.toString());
		}
		myAssert("ClassAnnotation", annotation.value());
	}

	@Test
	@TestAnnotation()
	public void test_class_boolean_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(booleanValue, annotation.booleanValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_char_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(charValue, annotation.charValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_byte_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(byteValue, annotation.byteValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_short_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(shortValue, annotation.shortValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_int_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(intValue, annotation.intValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_float_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(floatValue, annotation.floatValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_double_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(doubleValue, annotation.doubleValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_long_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(longValue, annotation.longValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_String_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(stringValue, annotation.stringValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_Enum_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(enumValue, annotation.enumValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_Class_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(classValue, annotation.classValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_Annotation_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(annotationValue, annotation.annotationValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_boolean_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(booleanArrayValue, annotation.booleanArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_char_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(charArrayValue, annotation.charArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_byte_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(byteArrayValue, annotation.byteArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_short_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(shortArrayValue, annotation.shortArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_int_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(intArrayValue, annotation.intArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_float_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(floatArrayValue, annotation.floatArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_double_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(doubleArrayValue, annotation.doubleArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_long_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(longArrayValue, annotation.longArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_String_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(stringArrayValue, annotation.stringArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_Enum_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(enumArrayValue, annotation.enumArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_Class_arrayannotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(classArrayValue, annotation.classArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_class_Annotation_array_annotation() throws Exception {
		ValueAnnotation annotation = (ValueAnnotation)testClass.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + testClass.toString());
		}
		myAssert(annotationArrayValue, annotation.annotationArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_annotations() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ConstructorAnnotation annotation = (ConstructorAnnotation)constructor
				.getAnnotation(ConstructorAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException(
					"ConstructorAnnotation not found on " + constructor.toGenericString());
		}
		myAssert("ConstructorAnnotation", annotation.value());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_no_annotations() throws Exception {
		Constructor<?> constructor = testClass.getConstructor();
		Annotation[] annotation = constructor.getDeclaredAnnotations();
		myAssert(noAnnotations, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_constructor_boolean_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(booleanValue, annotation.booleanValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_char_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(charValue, annotation.charValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_byte_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(byteValue, annotation.byteValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_short_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(shortValue, annotation.shortValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_int_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(intValue, annotation.intValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_float_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(floatValue, annotation.floatValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_double_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(doubleValue, annotation.doubleValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_long_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(longValue, annotation.longValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_String_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(stringValue, annotation.stringValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_Enum_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(enumValue, annotation.enumValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_Class_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(classValue, annotation.classValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_Annotation_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(annotationValue, annotation.annotationValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_boolean_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(booleanArrayValue, annotation.booleanArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_char_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(charArrayValue, annotation.charArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_byte_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(byteArrayValue, annotation.byteArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_short_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(shortArrayValue, annotation.shortArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_int_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(intArrayValue, annotation.intArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_float_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(floatArrayValue, annotation.floatArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_double_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(doubleArrayValue, annotation.doubleArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_long_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(longArrayValue, annotation.longArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_String_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(stringArrayValue, annotation.stringArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_Enum_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(enumArrayValue, annotation.enumArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_Class_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(classArrayValue, annotation.classArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_Annotation_array_annotation() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)constructor.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + constructor.toGenericString());
		}
		myAssert(annotationArrayValue, annotation.annotationArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_constructor_parameter_annotations() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class, String.class);
		Annotation[][] annotation = constructor.getParameterAnnotations();
		myAssert(parameterValueJava8, parameterValueJava9, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_constructor_no_parameter_annotations() throws Exception {
		Constructor<?> constructor = testClass.getConstructor(String.class);
		Annotation[][] annotation = constructor.getParameterAnnotations();
		myAssert(noParameterValue, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_constructor_empty_parameter_list() throws Exception {
		Constructor<?> constructor = testClass.getConstructor();
		Annotation[][] annotation = constructor.getParameterAnnotations();
		myAssert(emptyParameterListValue, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_method_annotations() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		MethodAnnotation annotation = (MethodAnnotation)method.getAnnotation(MethodAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("MethodAnnotation not found on " + method.toGenericString());
		}
		myAssert("MethodAnnotation", annotation.value());
	}

	@Test
	@TestAnnotation()
	public void test_method_no_annotations() throws Exception {
		Method method = testClass.getMethod("method");
		Annotation[] annotation = method.getDeclaredAnnotations();
		myAssert(noAnnotations, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_method_boolean_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(booleanValue, annotation.booleanValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_char_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(charValue, annotation.charValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_byte_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(byteValue, annotation.byteValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_short_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(shortValue, annotation.shortValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_int_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(intValue, annotation.intValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_float_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(floatValue, annotation.floatValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_double_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(doubleValue, annotation.doubleValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_long_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(longValue, annotation.longValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_String_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(stringValue, annotation.stringValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Enum_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(enumValue, annotation.enumValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Class_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(classValue, annotation.classValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Annotation_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(annotationValue, annotation.annotationValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_boolean_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(booleanArrayValue, annotation.booleanArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_char_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(charArrayValue, annotation.charArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_byte_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(byteArrayValue, annotation.byteArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_short_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(shortArrayValue, annotation.shortArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_int_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(intArrayValue, annotation.intArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_float_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(floatArrayValue, annotation.floatArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_double_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(doubleArrayValue, annotation.doubleArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_long_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(longArrayValue, annotation.longArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_String_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(stringArrayValue, annotation.stringArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Enum_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(enumArrayValue, annotation.enumArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Class_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(classArrayValue, annotation.classArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Annotation_array_annotation() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		ValueAnnotation annotation = (ValueAnnotation)method.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + method.toGenericString());
		}
		myAssert(annotationArrayValue, annotation.annotationArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_boolean_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("booleanValue", (Class[])null);
		myAssert(booleanValueDefault, ((Boolean)method.getDefaultValue()).booleanValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_char_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("charValue", (Class[])null);
		myAssert(charValueDefault, ((Character)method.getDefaultValue()).charValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_byte_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("byteValue", (Class[])null);
		myAssert(byteValueDefault, ((Byte)method.getDefaultValue()).byteValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_short_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("shortValue", (Class[])null);
		myAssert(shortValueDefault, ((Short)method.getDefaultValue()).shortValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_int_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("intValue", (Class[])null);
		myAssert(intValueDefault, ((Integer)method.getDefaultValue()).intValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_float_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("floatValue", (Class[])null);
		myAssert(floatValueDefault, ((Float)method.getDefaultValue()).floatValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_double_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("doubleValue", (Class[])null);
		myAssert(doubleValueDefault, ((Double)method.getDefaultValue()).doubleValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_long_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("longValue", (Class[])null);
		myAssert(longValueDefault, ((Long)method.getDefaultValue()).longValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_String_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("stringValue", (Class[])null);
		myAssert(stringValueDefault, (String)method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Enum_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("enumValue", (Class[])null);
		myAssert(enumValueDefault, (Enum)method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Class_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("classValue", (Class[])null);
		myAssert(classValueDefault, (Class)method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Annotation_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("annotationValue", (Class[])null);
		myAssert(annotationValueDefault, (ClassAnnotation)method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_boolean_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("booleanArrayValue", (Class[])null);
		myAssert(booleanArrayValueDefault, (boolean[])method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_char_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("charArrayValue", (Class[])null);
		myAssert(charArrayValueDefault, (char[])method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_byte_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("byteArrayValue", (Class[])null);
		myAssert(byteArrayValueDefault, (byte[])method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_short_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("shortArrayValue", (Class[])null);
		myAssert(shortArrayValueDefault, (short[])method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_int_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("intArrayValue", (Class[])null);
		myAssert(intArrayValueDefault, ((int[])method.getDefaultValue()));
	}

	@Test
	@TestAnnotation()
	public void test_method_float_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("floatArrayValue", (Class[])null);
		myAssert(floatArrayValueDefault, ((float[])method.getDefaultValue()));
	}

	@Test
	@TestAnnotation()
	public void test_method_double_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("doubleArrayValue", (Class[])null);
		myAssert(doubleArrayValueDefault, ((double[])method.getDefaultValue()));
	}

	@Test
	@TestAnnotation()
	public void test_method_long_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("longArrayValue", (Class[])null);
		myAssert(longArrayValueDefault, ((long[])method.getDefaultValue()));
	}

	@Test
	@TestAnnotation()
	public void test_method_String_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("stringArrayValue", (Class[])null);
		myAssert(stringArrayValueDefault, (String[])method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Enum_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("enumArrayValue", (Class[])null);
		myAssert(enumArrayValueDefault, (Enum[])method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Class_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("classArrayValue", (Class[])null);
		myAssert(classArrayValueDefault, (Class[])method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_Annotation_array_getDefaultValue() throws Exception {
		Method method = ValueAnnotation.class.getMethod("annotationArrayValue", (Class[])null);
		myAssert(annotationArrayValueDefault, (ClassAnnotation[])method.getDefaultValue());
	}

	@Test
	@TestAnnotation()
	public void test_method_parameter_annotations() throws Exception {
		Method method = testClass.getMethod("method", String.class, String.class);
		Annotation[][] annotation = method.getParameterAnnotations();
		myAssert(parameterValueJava8, parameterValueJava9, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_method_no_parameter_annotations() throws Exception {
		Method method = testClass.getMethod("method", String.class);
		Annotation[][] annotation = method.getParameterAnnotations();
		myAssert(noParameterValue, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_method_empty_parameter_list() throws Exception {
		Method method = testClass.getMethod("method");
		Annotation[][] annotation = method.getParameterAnnotations();
		myAssert(emptyParameterListValue, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_field_annotations() throws Exception {
		Field field = testClass.getField("field");
		FieldAnnotation annotation = (FieldAnnotation)field.getAnnotation(FieldAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("FieldAnnotation not found on " + field.toGenericString());
		}
		myAssert("FieldAnnotation", annotation.value());
	}

	@Test
	@TestAnnotation()
	public void test_field_no_annotations() throws Exception {
		Field field = testClass.getField("field1");
		Annotation[] annotation = field.getDeclaredAnnotations();
		myAssert(noAnnotations, annotation);
	}

	@Test
	@TestAnnotation()
	public void test_field_boolean_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(booleanValue, annotation.booleanValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_char_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(charValue, annotation.charValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_byte_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(byteValue, annotation.byteValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_short_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(shortValue, annotation.shortValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_int_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(intValue, annotation.intValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_float_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(floatValue, annotation.floatValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_double_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(doubleValue, annotation.doubleValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_long_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(longValue, annotation.longValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_String_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(stringValue, annotation.stringValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_Enum_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(enumValue, annotation.enumValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_Class_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(classValue, annotation.classValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_Annotation_annotation() throws Exception {
		int returnValue;
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(annotationValue, annotation.annotationValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_boolean_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(booleanArrayValue, annotation.booleanArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_char_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(charArrayValue, annotation.charArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_byte_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(byteArrayValue, annotation.byteArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_short_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(shortArrayValue, annotation.shortArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_int_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(intArrayValue, annotation.intArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_float_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(floatArrayValue, annotation.floatArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_double_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(doubleArrayValue, annotation.doubleArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_long_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(longArrayValue, annotation.longArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_String_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(stringArrayValue, annotation.stringArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_Enum_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(enumArrayValue, annotation.enumArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_Class_array_annotation() throws Exception {
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(classArrayValue, annotation.classArrayValue());
	}

	@Test
	@TestAnnotation()
	public void test_field_Annotation_array_annotation() throws Exception {
		int returnValue;
		Field field = testClass.getField("field");
		ValueAnnotation annotation = (ValueAnnotation)field.getAnnotation(ValueAnnotation.class);
		if (annotation == null) {
			throw new AnnotationNotFoundException("ValueAnnotation not found on " + field.toGenericString());
		}
		myAssert(annotationArrayValue, annotation.annotationArrayValue());
	}

	public static Vector<String> foo;

	private boolean myAssert(boolean a, boolean b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(char a, char b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(byte a, byte b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(short a, short b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(int a, int b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(float a, float b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(double a, double b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(long a, long b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(String a, String b) {
		boolean value = a.equals(b);
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(Enum a, Enum b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(Class a, Class b) {
		boolean value = a == b;
		if (!value) {
			logger.error(a + "!=" + b);
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(String a, ClassAnnotation b) {
		boolean value = b.value().equals(a);
		if (!value) {
			logger.error(a + "!=" + b.value());
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(boolean[] a, boolean[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(char[] a, char[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(byte[] a, byte[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(short[] a, short[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(int[] a, int[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(float[] a, float[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(double[] a, double[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(long[] a, long[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(String[] a, String[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (!a[i].equals(b[i])) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(Enum[] a, Enum[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(Class[] a, Class[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i] != b[i]) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(String[] a, ClassAnnotation[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (!b[i].value().equals(a[i])) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i].value() + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(String[] a, Annotation[] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (!b[i].equals(a[i])) {
				logger.error("(a[" + i + "]" + " = " + a[i] + "}!=(b[" + i + "]" + " = " + b[i] + "}");
				value = false;
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(String[][] a, Annotation[][] b) {
		boolean value = true;
		if (a.length != b.length) {
			Assert.fail("(a.length = " + a.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a.length; i++) {
			if (a[i].length != b[i].length) {
				logger.error("(a[" + i + "].length = " + a[i].length + "}!=(b[" + i + "].length=" + b[i].length + ")");
				return false;
			}
			for (int j = 0; j < a[i].length; j++) {
				if (!b[i][j].toString().equals(a[i][j])) {
					logger.error("(a[" + i + "][" + j + "] = " + a[i][j] + "}!=(b[" + i + "][" + j + "] = "
							+ b[i][j].toString() + "}");
					value = false;
				}
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}

	private boolean myAssert(String[][] a1, String[][] a2, Annotation[][] b) {
		boolean value = true;
		if (a1.length != a2.length) {
			Assert.fail("(a1.length = " + a1.length + "}!=(a2.length=" + a2.length + ")");
			return false;
		}
		if (a1.length != b.length) {
			Assert.fail("(a1.length = " + a1.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		if (a2.length != b.length) {
			Assert.fail("(a2.length = " + a2.length + "}!=(b.length=" + b.length + ")");
			return false;
		}
		for (int i = 0; i < a1.length; i++) {
			if (a1[i].length != b[i].length) {
				logger.error("(a1[" + i + "].length = " + a1[i].length + "}!=(b[" + i + "].length=" + b[i].length + ")");
				return false;
			}
			for (int j = 0; j < a1[i].length; j++) {
				if (!b[i][j].toString().equals(a1[i][j]) && !b[i][j].toString().equals(a2[i][j])) {
					logger.error("(a1[" + i + "][" + j + "] = " + a1[i][j] + "}!=(b[" + i + "][" + j + "] = "
							+ b[i][j].toString() + "} AND (a2[" + i + "][" + j + "] = " + a2[i][j] + "}!=(b[" + i + "][" + j + "] = "
									+ b[i][j].toString() + "}");
					value = false;
				}
			}
		}
		AssertJUnit.assertTrue(value);
		return value;
	}
}
