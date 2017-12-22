/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;

/**
 * <p>Represents an internal ClassLoader structure within a Java VM instance.
 * For most ClassLoaders there is a corresponding <code>java.lang.ClassLoader</code>
 * object within with JavaRuntime. For primordial class loaders such as
 * the bootstrap class loader, there may or may not be a corresponding
 * <code>java.lang.ClassLoader</code> instance.</p>
 * 
 * <p>Since Java does not define any strict inheritance structure between
 * class loaders, there are no APIs for inspecting 'child' or 'parent'
 * class loaders. This information may be inferred by inspecting the
 * corresponding <code>java.lang.ClassLoader</code> instance.</p>
 * 
 * @see java.lang.ClassLoader
 * 
 */
public interface JavaClassLoader {

    /**
     * Get the set of classes which are defined in this JavaClassLoader. 
     * @return an iterator over the collection of classes which are defined
     * in this JavaClassLoader 
     * 
     * @see JavaClass
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getDefinedClasses();
    
    /**
     * When a ClassLoader successfully delegates a findClass() request to
     * another ClassLoader, the result of the delegation must be cached within
     * the internal structure so that the VM does not make repeated requests
     * for the same class.
     * 
     * @return an iterator over the collection of classes which are defined
     * in this JavaClassLoader <i>or</i> which were found by delegation to
     * other JavaClassLoaders
     * 
     * @see JavaClass
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getCachedClasses();
    
    /**
     * Find a named class within this class loader. The class may have been
     * defined in this class loader, or this class loader may have delegated
     * the load to another class loader and cached the result.
     * 
     * @param name of the class to find. Packages should be separated by
     * '/' instead of '.'
     * @return the JavaClass instance, or null if it is not found
     * @throws CorruptDataException 
     */
    public JavaClass findClass(String name) throws CorruptDataException;
    
    /**
     * Get the java.lang.ClassLoader instance associated with this class loader.
     * @return a JavaObject representing the java.lang.ClassLoader instance
     * associated with this class loader, or null if there is no Java class
     * loader associated with this low-level class loader.
     * @throws CorruptDataException 
     * 
     * @see JavaObject
     * @see ClassLoader
     */
    public JavaObject getObject() throws CorruptDataException;
    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Class Loader in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
}
