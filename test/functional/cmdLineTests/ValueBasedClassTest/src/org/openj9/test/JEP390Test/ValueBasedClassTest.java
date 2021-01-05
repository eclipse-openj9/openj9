/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
package org.openj9.test.JEP390Test;
/**
 * This class is used by test cases in jep390.xml. jep390.xml has test cases checking for line numbers in the stack trace, 
 * so jep390.xml may need to be updated accordingly if this file is modified. 
 */
public class ValueBasedClassTest
{
	public static void main(String[] args) {
		ValueBasedClass vb = new ValueBasedClass(2021);
		String input1 = "VB";
		String input2 = "OBJ";
		if (args.length != 1) {
			System.out.println("Pass in either " + '"' + input1 + '"' + " or " + '"' + input2 + '"' + " as parameter");
		} else {
			if (args[0].equals(input1)) {
				syncOnValueBasedClass(vb);
			} else if (args[0].equals(input2)) {
				syncOnObject(vb);
			} else {
				System.out.println("Pass in either " + '"' + input1 + '"' + " or " + '"' + input2 + '"' + " as parameter");
			}
		}
		System.out.println("ValueBasedClassTest Exit");
	}
	private static void syncOnValueBasedClass(ValueBasedClass vb) {
		System.out.println("Calling method syncOnValueBasedClass()");
		synchronized (vb) {
			System.out.println("synchronizing on ValueBasedClass");
		}
	}
	private static void syncOnObject(Object o) {
		System.out.println("Calling method syncOnObject()");
		synchronized (o) {
			System.out.println("synchronizing on Object");
		}
	}
}
