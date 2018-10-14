package org.openj9.test.annotationClassLoader;

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
import java.lang.annotation.*;
import java.lang.reflect.*;

@Annotation1(myClass = HelperClass2.class)
public class AnnotationClass implements Runnable {

	@Annotation1()
	public void run() {
		Annotation[] annotations = getClass().getDeclaredAnnotations();
		// test all the cases in the invocation handler
		String result = annotations[0].toString();
		if ((result.indexOf("myClass=class org.openj9.test.annotationClassLoader.HelperClass2") == -1) &&
			(result.indexOf("myClass=org.openj9.test.annotationClassLoader.HelperClass2.class") == -1))
			throw new Error("unexpected: " + result);
		annotations[0].annotationType();
		annotations[0].hashCode();
		((Annotation1)annotations[0]).id();
		Class cls = ((Annotation1)annotations[0]).myClass();
		if (!cls.getName().equals("org.openj9.test.annotationClassLoader.HelperClass2")) {
			throw new Error("Wrong class loaded: " + cls);
		}
		try {
			Method method = getClass().getMethod("run");
			Annotation[] methodAnnotations = method.getDeclaredAnnotations();
			annotations[0].equals(methodAnnotations[0]);
		} catch (NoSuchMethodException e) {
			throw new Error(e);
		}
	}
}
