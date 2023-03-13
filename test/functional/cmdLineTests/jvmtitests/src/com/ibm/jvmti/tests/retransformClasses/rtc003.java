/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.jvmti.tests.retransformClasses;

import java.util.Map;
import java.util.TreeMap;
import java.util.function.Function;

public class rtc003 {
	public static native boolean retransformClass(Class originalClass);

	public boolean testRedefineImplementedInterface() {
		if (!retransformClass(Map.class)) {
			System.out.println("Failed to retransform class");
			return false;
		}

		TreeMap<String, String> treeMap = new TreeMap<>();                                     
		treeMap.computeIfAbsent("TestKey", new TestFunction());

		return true;
	}

	public String helpRedefineImplementedInterface() {
		return "Tests retransforming an implemented interface class";
	}

	private class TestFunction implements Function<String, String> {
		public String apply(String t) {
			return t + ": " + TestFunction.class.getName();
		}
	}
}
