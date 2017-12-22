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

import java.io.*;
/**
 *	A classloader which does not send defineClass() itself.  Instead, each time
 *	it receives findClass(), it creates a new SimpleJarClassLoader, to which
 *	it sends loadClass().  The temporary helper SimpleJarClassLoader, thus,
 *	defines the class.  
 *
 *		Named instances of DelegatingJarClassLoader report their own finalization to 
 *	the FinalizationIndicator class, which may be querried to determine whether any
 *	given instance has been finalized.
 *
 **/
public class DelegatingJarClassLoader extends ClassLoader {
	
	String jarfileName;
	Counter counterLock;
	String name, helperName;
	FinalizationIndicator finalizationIndicator;

public DelegatingJarClassLoader(String name, String helperName, String jarFileName) 
{
	this.counterLock = counterLock;
	this.jarfileName = jarFileName;
	this.helperName = helperName;
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
	try {
		ClassLoader cl = new SimpleJarClassLoader(helperName, jarfileName);
		return cl.loadClass(clsName);
	} catch(IOException e) {
		e.printStackTrace();
		throw new ClassNotFoundException(clsName);
	}
}

}
