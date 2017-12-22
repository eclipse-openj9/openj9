/*[INCLUDE-IF Sidecar16]*/
package com.ibm.oti.reflect;

/*******************************************************************************
 * Copyright (c) 2005, 2010 IBM Corp. and others
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
import java.lang.ref.SoftReference;

public class Constructor {
	private java.lang.reflect.Constructor constructor;
	private SoftReference<Annotations> annotations;
	
public Constructor(java.lang.reflect.Constructor constructor) {
	this.constructor = constructor;
}

private synchronized Annotations getAnnotations() {
	Annotations ans;
	if (annotations == null || (ans = annotations.get()) == null) {
		annotations = new SoftReference<Annotations>(ans = new Annotations(AnnotationParser.parseAnnotations(constructor)));
	}
	return ans;
}

public <T extends Annotation> T getAnnotation(Class<T> cl) {
	if (cl == null) throw new NullPointerException();
	return getAnnotations().getAnnotation(cl);
}

public Annotation[] getDeclaredAnnotations() {
	return getAnnotations().getAnnotations().clone();
}

public Annotation[][] getParameterAnnotations() {
	return AnnotationParser.parseParameterAnnotations(constructor);
}

}
