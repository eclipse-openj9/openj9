package org.openj9.test.typeAnnotation;

import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedType;
import java.lang.reflect.TypeVariable;
import java.util.Set;

import org.testng.log4testng.Logger;

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
/**
 * Test case to exercise type annotations on generics.
 * This generates a class file containing annotations in various locations specified by JSR 308 - Annotations on Types.
 */

public class AnnotatedTypeParameterTestClass < @TestAnn(site="typeParameterAnnotation") T extends @TestAnn(site="methodTypeBounds") Set > {

	private static final Logger logger = Logger.getLogger(AnnotatedTypeParameterTestClass.class);
	public static void main(String args[]) {
		Class c = AnnotatedTypeParameterTestClass.class;
		for (TypeVariable tv: c.getTypeParameters()) {
			logger.debug("type parameters annotations");
			for (Annotation an: tv.getAnnotations()) {
				logger.debug(an);
			}
			for (AnnotatedType ab: tv.getAnnotatedBounds()) {
				logger.debug("bounds annotations");

				for (Annotation an: ab.getAnnotations()) {
					logger.debug(an);
				}
			}
		}
	}
}
