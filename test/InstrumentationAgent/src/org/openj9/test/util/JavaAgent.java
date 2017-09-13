/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

package org.openj9.test.util;

import java.lang.instrument.ClassDefinition;
import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.Instrumentation;

public class JavaAgent {
	private static Instrumentation instrumentation = null;

	private static String agentArguments = null;

	public static void premain( String agentArgs, Instrumentation inst ) {
		instrumentation = inst;
		agentArguments = agentArgs;
	}

	public static void addTransformer(ClassFileTransformer transformer, boolean canRetransform) {
		instrumentation.addTransformer(transformer, canRetransform);
	}

	public static void redefineClass(Class<?> theClass, byte[] classBytes) throws Exception {
		ClassDefinition cdf = new ClassDefinition( theClass, classBytes );
		instrumentation.redefineClasses(cdf);
	}

	public static void retransformClass(Class<?> theClass) throws Exception {
		instrumentation.retransformClasses(theClass);
	}

	public static long getObjectSize(Object objectToSize) {
		return instrumentation.getObjectSize(objectToSize);
	}
}
