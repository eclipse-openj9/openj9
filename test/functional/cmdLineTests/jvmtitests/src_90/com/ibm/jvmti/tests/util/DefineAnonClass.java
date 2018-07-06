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
package com.ibm.jvmti.tests.util;
import java.io.DataInputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.security.ProtectionDomain;
import jdk.internal.misc.Unsafe;
import java.io.ByteArrayOutputStream;

/**
 * 
 * Helper Class to create anonClass
 *
 */
public class DefineAnonClass {
	private static Unsafe unsafe;
	static {
		try {
			Field f = Unsafe.class.getDeclaredField("theUnsafe");
			f.setAccessible(true);
			unsafe = (Unsafe) f.get(null);
		} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
			e.printStackTrace();
		}	
	}
	
	static public Class<?> defineAnonClass(String filePath, Class<?> hostClass) {
		byte[] bytes = getClassBytes(filePath);

		Class<?> anon = unsafe.defineAnonymousClass(hostClass, bytes, null);
		return anon;
	}
	
	static public Class<?> defineAnonClass(Class<?> anonClass, Class<?> hostClass) {		
		byte[] bytes = getClassBytesFromResource(anonClass);
	
		Class<?> anon = unsafe.defineAnonymousClass(hostClass, bytes, null);
		return anon;
	}
	
	static public Class<?> callDefineAnonClass(Class<?> hostClass, byte[] bytes, Object[] cpPatches) {
		return unsafe.defineAnonymousClass(hostClass, bytes, cpPatches);
	}
	
	
	static public Class<?> defineClass(String className, String byteCodePath, ClassLoader loader, ProtectionDomain pD) throws Throwable {
		byte[] bytes = getClassBytes(byteCodePath);

		Class<?> newClass = unsafe.defineClass(className, bytes, 0, bytes.length, loader, pD);
		return newClass;
	}
	
	static public byte[] getClassBytes(String fileName) {
		String classFile = fileName.replace(".", "/")+ ".class";
		InputStream classStream = null;
		byte classBytes[] = null;
		
		try {
			classStream = new FileInputStream(classFile);
		} catch (FileNotFoundException e) {
			new RuntimeException("Invalid file name");
		}

		try {
			int size = classStream.available();
			classBytes = new byte[size];
			DataInputStream in = new DataInputStream(classStream);
			in.readFully(classBytes);
			in.close();
		} catch (IOException e) {
			new RuntimeException("Error reading in: " + fileName);
		}

		return classBytes;
	}
	
	static public byte[] getClassBytesFromResource(Class<?> anonClass) {
		String className = anonClass.getName();
		String classAsPath = className.replace('.', '/') + ".class";
		InputStream classStream = anonClass.getClassLoader().getResourceAsStream(classAsPath);
		ByteArrayOutputStream baos = new ByteArrayOutputStream();

		int read;
		byte[] buffer = new byte[16384];
		byte[] result = null;
		try {
			while ((read = classStream.read(buffer, 0, buffer.length)) != -1) {
				baos.write(buffer, 0, read);
			}
			result = baos.toByteArray();
		} catch (IOException e) {
			throw new RuntimeException("Error reading in resource: " + anonClass.getName(), e);
		}
		return result;
	}
}
