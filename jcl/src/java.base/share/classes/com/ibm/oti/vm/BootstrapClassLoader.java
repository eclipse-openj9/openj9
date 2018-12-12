/*[INCLUDE-IF Sidecar16 & !Sidecar19-SE]*/

package com.ibm.oti.vm;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

import java.util.*;

import com.ibm.oti.util.PriviAction;

import java.io.FilePermission;
import java.lang.reflect.Method;
import java.security.AccessController;
import java.security.PrivilegedAction;

/**
 * BootstrapClassLoaders load classes out of the file system,
 * from the directories and jars on the system class path.
 * The system class path can be set using the command line 
 * option "-scp <paths>", and can be read from the system
 * property "sun.boot.class.path".
 *
 * @author		OTI
 * @version		initial
 */

/*[PR 1FDT6UF]*/
public final class BootstrapClassLoader extends AbstractClassLoader {

	private static BootstrapClassLoader singleton;
	private static Method appendToClassPathForInstrumentationMethod = null;
	private static boolean initAppendMethod = true;

/**
 * Prevents this class from being instantiated.
 * BootstrapClassLoaders may only be created by the VM.
 *
 * @author		OTI
 * @version		initial
 */
private BootstrapClassLoader() {
	int count = VM.getClassPathCount();
	types = new int[count];
	cache = new Object[count];
	parsedPath = new String[count];
	VM.initializeClassLoader(this, VM.J9_CLASSLOADER_TYPE_BOOT, false);
}

/**
 * Invoked by the Virtual Machine when resolving class references.
 * Equivalent to loadClass(className, false);
 *
 * @return 		java.lang.Class
 *					the Class object.
 * @param 		className String
 *					the name of the class to search for.
 *
 * @exception	ClassNotFoundException
 *					If the class could not be found.
 */
/*[PR 95894]*/
@Override
public Class<?> loadClass(String className) throws ClassNotFoundException {
	/*[PR 111332] synchronization required for JVMTI */
	/*[PR VMDESIGN 1433] Remove Java synchronization (Prevent redundant loads of the same class) */
	Class<?> loadedClass = VM.getVMLangAccess().findClassOrNullHelper(className, this);
	return loadedClass;
}

public static ClassLoader singleton() {
	if (singleton == null)
	 	singleton = new BootstrapClassLoader();
	 else
/*[MSG "K0084", "can only instantiate one BootstrapClassLoader"]*/
	 	throw new SecurityException(com.ibm.oti.util.Msg.getString("K0084")); //$NON-NLS-1$
 	return singleton;
}

protected Package getPackage(String name) {
	return VM.getVMLangAccess().getSystemPackage(name);
}

protected Package[] getPackages() {
	return VM.getVMLangAccess().getSystemPackages();	
}

/*[PR 123807] Design 450 SE.JVMTI: JVMTI 1.1: New ClassLoaderSearch API */
/**
 * Invoked by the Virtual Machine when a JVMTI agent wishes to add to the bootstrap class path.
 * The VM has already verified that the jar/zip exists and is superficially valid.
 *
 * @param 		className jarPath
 *					the name of the jar/zip to add.
 */
private void appendToClassPathForInstrumentation(String jarPath) throws Throwable {
	synchronized(cacheLock) {
		// Note: No java-level synchronization is required to call the native.
		/*[PR CMVC 164017] appendToClassPathForInstrumentation uses UTF8 file path */
		int newCount = addJar(com.ibm.oti.util.Util.getBytes(jarPath));
		
		int[] newTypes = new int[newCount];
		System.arraycopy(types, 0, newTypes, 0, newCount - 1);
		types = newTypes;

		Object[] newCache = new Object[newCount];
		System.arraycopy(cache, 0, newCache, 0, newCount - 1);
		cache = newCache;

		String[] newParsedPath = new String[newCount];
		System.arraycopy(parsedPath, 0, newParsedPath, 0, newCount - 1);
		parsedPath = newParsedPath;

		if (permissions != null) {
			FilePermission[] newPermissions = new FilePermission[newCount];
			System.arraycopy(permissions, 0, newPermissions, 0, newCount -1);
			permissions = newPermissions;
		}
		// clear the getResources() cache when a jar is appended
		resourceCacheRef = null;
	}
}

/**
 * Add a JAR/ZIP to the boot class path, creating a new class path entry for it.
 *
 * @return 		int
 *					the new size of the class path entry array
 * @param 		jarPath byte[]
 *					the name of the jar/zip to add, in OS filename encoding
 */
private native int addJar(byte jarPath[]);

}
