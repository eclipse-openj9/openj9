/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.test.stackwalker.testapp;

/**
 * A test application that tries to generate a core dump with a resolve frame in it.
 * 
 * Generates a generic special frame with -Xint, and a static resolve frame with -Xjit:count=0
 * 
 * @author andhall
 *
 */
public class TestApp2
{

	private static void method1(Object o)
	{
		TestApp2_2.method2(7, 42L, 3.14159f, 2.718438584d,new Object());
	}
	
	/**
	 * @param args
	 */
	public static void main(String[] args)
	{
		method1(new Object());
	}

}
