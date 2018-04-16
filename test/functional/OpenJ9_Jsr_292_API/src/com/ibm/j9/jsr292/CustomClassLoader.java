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
package com.ibm.j9.jsr292;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Helper class loader used in JSR 292 tests for MethodHandles.Lookup API 
 */
public class CustomClassLoader extends ClassLoader {
	
	public CustomClassLoader(ClassLoader parent) {
		super(parent);
	}
	
	@Override
	public Class<?> loadClass(String className) throws ClassNotFoundException {
		if (className.equals("com.ibm.j9.jsr292.CustomLoadedClass1") || className.equals("com.ibm.j9.jsr292.CustomLoadedClass2")) {
			return getClass(className);
		}
		return super.loadClass(className);
	}
	
	private Class<?> getClass(String className)throws ClassNotFoundException {
		String classFile = className.replace(".", "/")+ ".class";
		try {
			InputStream classStream = getClass().getClassLoader().getResourceAsStream(classFile);
			if (classStream == null) {
				throw new ClassNotFoundException("Error loading class : " + classFile);
			}
	        int size = classStream.available();
	        byte classRep[] = new byte[size];
	        DataInputStream in = new DataInputStream(classStream);
	        in.readFully(classRep);
	        in.close();
	        Class<?> clazz = defineClass(className, classRep, 0, classRep.length);
			return clazz;
		} catch (IOException e) {
			throw new ClassNotFoundException(e.getMessage());
		} 
	}
}	
