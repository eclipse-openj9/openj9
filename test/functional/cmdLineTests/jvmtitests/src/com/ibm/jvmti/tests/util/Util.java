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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

public class Util {
	public static boolean testClassLoading(String name, ClassLoader loader) {
		boolean rc = false;
		try {
			Class.forName(name, false, loader);
			rc = true;
		} catch (Throwable e) {
			System.err.println("Unable to load class " + name + " in " + loader);
			e.printStackTrace();
		}
		return rc;
	}

	public static boolean loadClassAndResource(String name, ClassLoader loader) {
		boolean rc = false;

		try {
			Class c = Class.forName(name, false, loader);
			String s = '/' + name.replace('.', '/') + ".java";
			try {
				InputStream i = c.getResourceAsStream(s);
				if (i == null) {
					System.err.println("Unable to retrieve resource " + s + " in " + loader);
				} else {
					i.close();
					rc = true;
				}
			} catch (Throwable e) {
				System.err.println("Unable to retrieve resource " + s + " in " + loader);
				e.printStackTrace();
			}
		} catch (Throwable e) {
			System.err.println("Unable to load class " + name + " in " + loader);
			e.printStackTrace();
		}

		return rc;
	}
	
	public static byte[] readFile(String filename) throws IOException
	{
		RandomAccessFile file = new RandomAccessFile(filename, "r");
		int size = (int) file.length();
	        
		byte[] b = new byte[size];
		while (size > 0) {
			size -= file.read(b, b.length - size, size);
		}
	     
		file.close(); 			
		
		return b;	     
	}

	// returns the index >= offset of the first occurrence of sequence in buffer
	public static int indexOf(byte[] buffer, int offset, byte[] sequence)
	{
		while (buffer.length - offset >= sequence.length) {
			for (int i = 0; i < sequence.length; i++) {
				if (buffer[offset + i] != sequence[i]) {
					break;
				}
				if (i == sequence.length - 1)
					return offset;
			}
			offset++;
		}
		return -1;
	}

	// replaces all occurrences of seq1 in original buffer with seq2
	public static byte[] replaceAll(byte[] buffer, byte[] seq1, byte[] seq2) {
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		int index, offset = 0;
		while ((index = indexOf(buffer, offset, seq1)) != -1) {
			if (index - offset != 0) {
				out.write(buffer, offset, index - offset);
			}
			out.write(seq2, 0, seq2.length);
			offset = index + seq1.length;
		}
		if (offset < buffer.length) {
			out.write(buffer, offset, buffer.length - offset);
		}
		return out.toByteArray();
	}
	
	/* Retrieves the simple name from fully qualified class name. Expected to be same as returned by Class.getSimpleName() */
	public static String getSimpleName(String className) {
		int index;
		if((index = className.lastIndexOf('.')) != -1) {
			return className.substring(index+1, className.length());
		} else {
			return className;
		}
	}
	
	// retrieve the raw class file bytes for a given class name from its own jar file
	public static byte[] getClassBytes(String className)
	{
		byte classBytes[];

		try {
			URL url = Util.class.getProtectionDomain().getCodeSource().getLocation();
			String qualifiedClassName = className.replace('.', '/') + ".class";
			if (url.getFile().toLowerCase().endsWith(".jar")) {
				JarFile jar = new JarFile(new File(new URI(url.toExternalForm())));
				JarEntry entry = jar.getJarEntry(qualifiedClassName);
				InputStream in = jar.getInputStream(entry);
				ByteArrayOutputStream out = new ByteArrayOutputStream();
				int b;
				while ((b = in.read()) != -1) {
					out.write(b);
				}
				in.close();
				classBytes = out.toByteArray();
			} else {
				classBytes = null;
			}
		} catch (IOException e) {			
			e.printStackTrace();
			return null;
		} catch (URISyntaxException e) {
			e.printStackTrace();
			return null;
		}

		return classBytes;
	}

	// retrieve the raw class file bytes for a given Class
	public static byte[] getClassBytes(Class klass)
	{
		byte classBytes[];

		try {
			URL url = klass.getProtectionDomain().getCodeSource().getLocation();
			String qualifiedClassName = klass.getCanonicalName().replace('.', '/') + ".class";
			if (url.getFile().toLowerCase().endsWith(".jar")) {
				JarFile jar = new JarFile(new File(new URI(url.toExternalForm())));
				JarEntry entry = jar.getJarEntry(qualifiedClassName);
				InputStream in = jar.getInputStream(entry);
				ByteArrayOutputStream out = new ByteArrayOutputStream();
				int b;
				while ((b = in.read()) != -1) {
					out.write(b);
				}
				in.close();
				classBytes = out.toByteArray();
			} else {
				classBytes = Util.readFile(url.getPath() + "/" + qualifiedClassName);
			}
		} catch (IOException e) {			
			e.printStackTrace();
			return null;
		} catch (URISyntaxException e) {
			e.printStackTrace();
			return null;
		}

		return classBytes;
	}

	// retrieve the raw class file bytes for a given Class
	public static byte[] getClassBytesFromUrl(Class klass,URL url)
	{
		byte classBytes[];

		try {
			System.out.println(url.toString());
			String qualifiedClassName = klass.getCanonicalName().replace('.', '/') + ".class";
			if (url.getFile().toLowerCase().endsWith(".jar")) {
				JarFile jar = new JarFile(new File(new URI(url.toExternalForm())));
				JarEntry entry = jar.getJarEntry(qualifiedClassName);
				InputStream in = jar.getInputStream(entry);
				ByteArrayOutputStream out = new ByteArrayOutputStream();
				int b;
				while ((b = in.read()) != -1) {
					out.write(b);
				}
				in.close();
				classBytes = out.toByteArray();
			} else {
				classBytes = Util.readFile(url.getPath() + "/" + qualifiedClassName);
			}
		} catch (IOException e) {			
			e.printStackTrace();
			return null;
		} catch (URISyntaxException e) {
			e.printStackTrace();
			return null;
		}

		return classBytes;
	}
	
	public static byte[] replaceClassNames(byte[] classBytes, String originalClassName, String redefinedClassName) {
		// replace all occurrences of the redefined class name with the original class name
		try {
			byte[] r = redefinedClassName.getBytes("UTF-8");
			byte[] o = originalClassName.getBytes("UTF-8");
			// since this is a naive search and replace that doesn't
			// take into account the structure of the class file, and
			// hence would not update length fields, the two names must
			// be of the same length
			if (r.length != o.length) {
				return null;
			}
			classBytes = Util.replaceAll(classBytes, r, o);
		} catch (UnsupportedEncodingException e) {
			e.printStackTrace();
			return null;
		}
		return classBytes;
	}

	public static byte[] loadRedefinedClassBytesWithOriginalClassName(Class originalClass, Class redefinedClass)
	{
		byte classBytes[] = Util.getClassBytes(redefinedClass);

		if (classBytes != null) {
			String originalClassName = originalClass.getSimpleName();
			String redefinedClassName = redefinedClass.getSimpleName();
			classBytes = replaceClassNames(classBytes, originalClassName, redefinedClassName);
		}

		return classBytes;
	}

	public static boolean redefineClass(Class<?> testClass, Class<?> class1, Class<?> class2)
	{
		byte[] classBytes = Util.loadRedefinedClassBytesWithOriginalClassName(class1, class2);
		if (classBytes == null) {
			System.out.println("ERROR: Could not load redefined class bytes");
			return false;
		}

		Method redefineMethod = null;
		try {
			redefineMethod = testClass.getMethod("redefineClass", new Class<?>[] { Class.class, int.class, byte[].class });
		} catch (SecurityException e1) {
			e1.printStackTrace();
		} catch (NoSuchMethodException e1) {
			e1.printStackTrace();
		}
		if (redefineMethod == null) {
			System.out.println("ERROR: Could not find redefineClass() method in test class " + testClass.getSimpleName());
			return false;
		}

		boolean redefined = false;
		try {
			redefined = (Boolean) redefineMethod.invoke(null, new Object[] { class1, classBytes.length, classBytes });
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
		if (!redefined) {
			System.out.println("ERROR: Redefine failed");
			return false;
		}

		return true;
	}
	
	public static boolean redefineMultipleClass(Class<?> testClass, Class<?>[] original, Class<?>[] replacement)
	{
		int classCount = original.length;
		
		byte[][] classBytes = new byte[classCount][];
		int[] classBytesLength = new int[classCount];
		
		if (original.length != replacement.length) {
			System.out.println("ERROR: number of classes to be redefined (" + replacement.length + ") does not match the number of original class ("+ original.length +")");
			return false;
		}
		
		for (int i = 0; i < classCount; i++) {
			classBytes[i] = Util.loadRedefinedClassBytesWithOriginalClassName(original[i], replacement[i]);
			
			if (classBytes[i] == null) {
				System.out.println("ERROR: Could not load redefined class bytes");
				return false;
			}
			classBytesLength[i] = classBytes[i].length;
		}
		
		Method redefineMethod = null;
		try {
			redefineMethod = testClass.getMethod("redefineMultipleClass", new Class<?>[] { Class[].class, int.class, byte[][].class, int[].class });
		} catch (SecurityException e1) {
			e1.printStackTrace();
		} catch (NoSuchMethodException e1) {
			e1.printStackTrace();
		}
		if (redefineMethod == null) {
			System.out.println("ERROR: Could not find redefineClass() method in test class " + testClass.getSimpleName());
			return false;
		}

		boolean redefined = false;
		try {
			redefined = (Boolean) redefineMethod.invoke(null, new Object[] { original, classCount, classBytes, classBytesLength });
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
		if (!redefined) {
			System.out.println("ERROR: Redefine failed");
			return false;
		}

		return true;
	}
	
	public static boolean redefineClassExpectFailure(Class<?> testClass, Class<?> class1, Class<?> class2)
	{
		byte[] classBytes = Util.loadRedefinedClassBytesWithOriginalClassName(class1, class2);
		if (classBytes == null) {
			System.out.println("ERROR: Could not load redefined class bytes");
			return false;
		}

		Method redefineMethod = null;
		try {
			redefineMethod = testClass.getMethod("redefineClassExpectFailure", new Class<?>[] { Class.class, int.class, byte[].class });
		} catch (SecurityException e1) {
			e1.printStackTrace();
		} catch (NoSuchMethodException e1) {
			e1.printStackTrace();
		}
		if (redefineMethod == null) {
			System.out.println("ERROR: Could not find redefineClass() method in test class " + testClass.getSimpleName());
			return false;
		}

		boolean redefined = false;
		try {
			redefined = (Boolean) redefineMethod.invoke(null, new Object[] { class1, classBytes.length, classBytes });
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
		if (!redefined) {
			return false;
		}

		return true;
	}
	
	public static boolean redefineClassWithBytesExpectFailure(Class<?> testClass, Class<?> class1, byte[] classBytes)
	{
		if (classBytes == null) {
			System.out.println("ERROR: Could not load redefined class bytes");
			return false;
		}

		Method redefineMethod = null;
		try {
			redefineMethod = testClass.getMethod("redefineClassExpectFailure", new Class<?>[] { Class.class, int.class, byte[].class });
		} catch (SecurityException e1) {
			e1.printStackTrace();
		} catch (NoSuchMethodException e1) {
			e1.printStackTrace();
		}
		if (redefineMethod == null) {
			System.out.println("ERROR: Could not find redefineClass() method in test class " + testClass.getSimpleName());
			return false;
		}

		boolean redefined = false;
		try {
			redefined = (Boolean) redefineMethod.invoke(null, new Object[] { class1, classBytes.length, classBytes });
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
		if (!redefined) {
			return false;
		}

		return true;
	}
}
