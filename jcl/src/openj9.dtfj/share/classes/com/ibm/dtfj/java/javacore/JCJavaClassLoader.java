/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java.javacore;

import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaObject;

public class JCJavaClassLoader implements JavaClassLoader {
	
	private final ImagePointer fID;
	
	private LinkedHashMap fClassNames;
	private JCJavaRuntime fRuntime;
	private JCJavaObject fObject;
	
	public JCJavaClassLoader(JCJavaRuntime javaRuntime, long id) throws JCInvalidArgumentsException {
		if (javaRuntime == null) {
			throw new JCInvalidArgumentsException("Must pass a valid runtime.");
		}
		if (!javaRuntime.getImageProcess().getImageAddressSpace().isValidAddressID(id)) {
			throw new JCInvalidArgumentsException("Must pass a valid class loader id");
		}
		fRuntime = javaRuntime;
		fID = fRuntime.getImageProcess().getImageAddressSpace().getPointer(id);
		fClassNames = new LinkedHashMap();
		fObject = null;
		fRuntime.addJavaClassLoader(this);
	}

	/**
	 * 
	 */
	public JavaClass findClass(String className) {
		JavaClass foundClass = null;
		if (fClassNames.containsKey(className)) {
			ImagePointer ip = (ImagePointer)fClassNames.get(className);
			if (ip != null) {
				// By class ID
				long id = ip.getAddress();
				foundClass = fRuntime.findJavaClass(id);
			} else {
				// By class name
				foundClass = fRuntime.findJavaClass(className);
			}
		}
		return foundClass;
	}
	
	/**
	 * TODO: javacore appears to only list defined classes per class loader. If this changes
	 * in the future, this implementation must be changed.
	 */
	public Iterator getCachedClasses() {
		return getClasses();
	}

	/**
	 * 
	 */
	public Iterator getDefinedClasses() {
		return getClasses();
	}
	
	/**
	 * 
	 * 
	 */
	private Iterator getClasses() {
		Vector classes = new Vector();
		Iterator classNameIt = fClassNames.keySet().iterator();
		for(;classNameIt.hasNext();) {
			final Object next = classNameIt.next();
			classes.add(findClass((String)next));
		}
		return classes.iterator();
	}

	/**
	 * 
	 */
	public JavaObject getObject() throws CorruptDataException {
		return fObject;
	}
	
	/**
	 * 
	 * @param object
	 */
	public void setObject(JCJavaObject object) {
		fObject = object;
	}
	
	
	
	/**
	 * NON-DTFJ. For internal building purposes only.
	 * @param className
	 * 
	 */
	public JCJavaClass internalGetClass(String className) {
		return (JCJavaClass) findClass(className);
	}
	
	/**
	 * NON-DTFJ, don't use outside DTFJ. For internal building purposes only.
	 * 
	 */
	public JCJavaObject getInternalObject() {
		return fObject;
	}
	
	/**
	 * NOT in DTFJ
	 * @param name
	 */
	public void addClass(String name) {
		if (name != null && !fClassNames.containsKey(name)) {
			fClassNames.put(name, null);
		}
	}
		
	/**
	 * NOT in DTFJ
	 * @param name
	 */
	public void addClass(String name, ImagePointer ip) {
		fClassNames.put(name, ip);
	}
	
	/**
	 * NON-DTFJ
	 * 
	 */
	public ImagePointer getPointerID() {
		return fID;
	}
}
