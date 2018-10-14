/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.classunloading;

import java.util.jar.*;
import java.io.*;

/**
 *	A simple classloader which loads from one jar file.
 *
 *		Named instances of SimpleJarClassLoader report their own finalization to 
 *	the FinalizationIndicator class, which may be querried to determine whether any
 *	given instance has been finalized.
 **/
public class SimpleJarClassLoader extends ClassLoader {
	
	public static boolean instanceFinalized = false;
	
	JarFile jarfile;
	String name;
	FinalizationIndicator finalizationIndicator;

public SimpleJarClassLoader(String name, String jarFileName) 
	throws IOException
{
	initialize(name, jarFileName);
}
public SimpleJarClassLoader(String name, String jarFileName, ClassLoader parent) 
	throws IOException
{
	super(parent);
	initialize(name, jarFileName);
}
void initialize(String name, String jarFileName) 
	throws IOException
{
	jarfile = new JarFile(jarFileName);
	finalizationIndicator = new FinalizationIndicator(this.name = name);
}

public Class loadClass(String clsName)
	throws ClassNotFoundException
{
	//System.out.println(name + " loadClass() --- class " + clsName);
	return super.loadClass(clsName);
}

protected Class findClass(String clsName)
   	throws ClassNotFoundException
{
	//System.out.println(name + " findClass() --- class " + clsName);
	final String name = clsName.replace('.', '/') + ".class";

	InputStream is = null;
	try{
			JarEntry entry = jarfile.getJarEntry(name);
			if (entry != null) {
				is = jarfile.getInputStream(entry);
			}
	} catch (IOException e) {
		e.printStackTrace();
		throw new ClassNotFoundException(clsName);
	} 
	if (is != null) {
		byte[] clBuf;
		try {
			clBuf = getBytes(is);
			is.close();
		} catch (IOException e) {
			e.printStackTrace();
			throw new ClassNotFoundException(clsName);
		}
		return defineClass(clsName, clBuf, 0, clBuf.length);
	}
	throw new ClassNotFoundException(clsName);
}
private static byte[] getBytes(InputStream is) throws IOException {
	byte[] buf = new byte[4096];
	ByteArrayOutputStream bos = new ByteArrayOutputStream();
	int count;
	while ((count = is.read(buf)) > 0)
		bos.write(buf, 0, count);
	return bos.toByteArray();
}
	
}
