/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.java.diagnostics.utils.plugins;

import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/**
 * Information about the structural aspects of a class such as the annotations found and interfaces
 * supported.
 * 
 * @author adam
 *
 */
public class ClassInfo {
	private final List<String> ifaces = new ArrayList<String>();
	private final List<Annotation> annotations = new ArrayList<Annotation>();
	private final String classname;		//fully qualified name of this class
	private final URL url;				//where the class was loaded from
	private String superclass = null;	//super class name
	
	public ClassInfo(String classname, URL url) {
		this.classname = classname;
		this.url = url;
	}
	
	public List<String> getInterfaces() {
		return ifaces;
	}
	
	public void addInterface(String iface) {
		ifaces.add(iface);
	}
	
	public List<Annotation> getAnnotations() {
		return annotations;
	}
	
	public boolean hasInterface(Class<?> iface) {
		return ifaces.contains(iface.getName());
	}
	
	public boolean hasInterface(String iface) {
		return ifaces.contains(iface);
	}
	
	public boolean hasAnnotation(String classname) {
		Annotation annotation = new Annotation(classname);
		return annotations.contains(annotation);
	}
	
	public Annotation getAnnotation(String classname) {
		Annotation annotation = new Annotation(classname);
		int index = annotations.indexOf(annotation);
		if(index == -1) {
			return null;
		}
		return annotations.get(index);
	}
	
	/**
	 * Adds an annotation to the list
	 * @param classname class name for the annotation
	 * @return a new annotation if it did previously not exist, otherwise a new one is created
	 */
	public Annotation addAnnotation(String classname) {
		int index = annotations.indexOf(classname);
		if(index == -1) {
			Annotation a = new Annotation(classname);
			annotations.add(a);
			return a;
		} else {
			return annotations.get(index);
		}
	}

	public String getClassname() {
		return classname;
	}
	
	public URL getURL() {
		return url;
	}

	public String getSuperclass() {
		return superclass;
	}

	public void setSuperclass(String superclass) {
		this.superclass = superclass;
	}

	@Override
	public boolean equals(Object o) {
		if((o == null) || !(o instanceof ClassInfo)) {
			return false;
		}
		ClassInfo compareTo = (ClassInfo) o;
		return classname.equals(compareTo.classname);
	}

	@Override
	public int hashCode() {
		return classname.hashCode();
	}
	
	
	
}
