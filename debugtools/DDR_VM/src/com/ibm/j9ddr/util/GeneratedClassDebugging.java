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
package com.ibm.j9ddr.util;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/**
 * Utility functions for debugging generated classes
 * 
 * @author andhall
 *
 */
public class GeneratedClassDebugging
{

	/**
	 * Dumps the fields and methods of a class to the console.
	 * 
	 * Similar to javap
	 * @param clazz
	 */
	public static void dumpClass(Class<?> clazz) 
	{
		System.err.println("Dump of class: " + clazz.getName());
		
		System.err.println("Methods:");
		for (Method m : clazz.getMethods()) {
			System.err.println("* " + m);
		}
		
		System.err.println("Fields:");
		for (Field f : clazz.getFields()) {
			if ((f.getModifiers() & (Modifier.STATIC | Modifier.PUBLIC)) == (Modifier.STATIC | Modifier.PUBLIC)) {
				try {
					System.err.println("* " + f + " = " + f.get(null));
				} catch (Exception e) {
					throw new RuntimeException(e);
				}
			} else {
				System.err.println("* " + f);
			}
		}
	}
}
