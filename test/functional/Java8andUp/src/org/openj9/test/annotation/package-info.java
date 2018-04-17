@PackageAnnotation(value = "PackageAnnotation")
@ValueAnnotation(
		value = "ValueAnnotation",
		booleanValue = true,
		charValue = 'm',
		byteValue = -123,
		shortValue = -12345,
		intValue = -1234567890,
		floatValue = -12345.6789f,
		doubleValue = -12345.6789,
		longValue = -1234567890123456789l,
		stringValue = "valueString",
		enumValue = EnumClass.Value3,
		classValue = AnnotationTestClass.class,
		annotationValue = @ClassAnnotation(value = "ClassAnnotation"),
		booleanArrayValue = { false, true },
		charArrayValue = { 'm', 'q' },
		byteArrayValue = { -123, 123 },
		shortArrayValue = { -12345, 12345 },
		intArrayValue = { -1234567890, 1234567890 },
		floatArrayValue = { -12345.6789f, 12345.6789f },
		doubleArrayValue = { -12345.6789, 12345.6789 },
		longArrayValue = { -1234567890123456789l, 1234567890123456789l },
		stringArrayValue = { "valueString", "stringValue" },
		enumArrayValue = { EnumClass.Value2, EnumClass.Value1 },
		classArrayValue = { EnumClass.class, AnnotationTestClass.class },
		annotationArrayValue = {
				@ClassAnnotation(value = "ClassAnnotation1"),
				@ClassAnnotation(value = "ClassAnnotation2") })
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
