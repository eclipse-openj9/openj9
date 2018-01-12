/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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

package com.ibm.j9.recreateclass.testclasses;

/**
 * A test class which has 
 * 	- RuntimeInvisibleAnnotations attribute for ClassFile, method_info and field_info
 *  - RuntimeInvisibleParameterAnnotations for method_info
 *  
 * @author ashutosh
 *
 */
@RuntimeInvisibleAnnotations(value="ClassAnnotation1")	/* RuntimeInvisibleAnnotation in ClassFile */
@RuntimeVisibleAnnotation1(value="ClassAnnotation2")	/* RuntimeVisibleAnnotation in ClassFile */
public class MixedAnnotationsTest {
	@RuntimeInvisibleAnnotations(value="FieldAnnotation1")	/* RuntimeInvisibleAnnotation in field_info */
	@RuntimeVisibleAnnotation1(value="FieldAnnotation2")	/* RuntimeVisibleAnnotation in field_info */
	int field;
	
	@RuntimeInvisibleAnnotations(value="MethodAnnotation1")	/* RuntimeInvisibleAnnotation in method_info */
	@RuntimeVisibleAnnotation1(value="MethodAnnotation2")	/* RuntimeVisibleAnnotation in method_info */
	public void foo(
			@RuntimeInvisibleAnnotations(value="ParameterAnnotation1") /* RuntimeInvisibleParameterAnnotation in method_info */
			@RuntimeVisibleAnnotation1(value="ParameterAnnotation2") /* RuntimeVisibleParameterAnnotation in method_info */
			String param) {	
		return;
	}
}

