/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package org.openj9.test.NoSuchMethod;

import java.lang.invoke.MethodType;

class TestInfo {
	public MethodType methodType;
	public String methodName;
	public String[] expectedGroups;
	public ClassInfo calledClass;
	public ClassInfo callingClass;

	public TestInfo(ClassInfo calledClass, String methodName, MethodType methodType, ClassInfo callingClass) {
		this.calledClass = calledClass;
		this.methodName = methodName;
		this.methodType = methodType;
		this.callingClass = callingClass;
		initExpectedGroups();
	}

	public TestInfo(ClassInfo callingClass, ClassInfo calledClass, String methodName, MethodType methodType) {
		this.calledClass = calledClass;
		this.methodName = methodName;
		this.methodType = methodType;
		this.callingClass = callingClass;
		initExpectedGroups();
	}

	public TestInfo(ClassInfo calledClass, String methodName, MethodType methodType, String callingClassName,
			CustomClassLoader cl) {
		this(calledClass, methodName, methodType,
				new ClassInfo(cl.buildInvoker(callingClassName, calledClass.getName(), methodName, methodType)));
	}

	private void initExpectedGroups() {
		String[] tmp = {
			null, null,
			"called class", calledClass.getName().replace('.', '/'),
			"method name", methodName,
			"method signature", methodType.toMethodDescriptorString(),
			"called class path", calledClass.getClassPath(),
			"called class loader", calledClass.getClassLoader(),
			"calling class", callingClass.toString(),
			"calling class path", callingClass.getClassPath(),
			"calling class loader", callingClass.getClassLoader() };
		expectedGroups = tmp;
	}

	public String[] getExpectedGroups() {
		return expectedGroups;
	}

	public String toString() {
		return "Testcase " + callingClass.getName() + " calls " + calledClass.getName();
	}
}
