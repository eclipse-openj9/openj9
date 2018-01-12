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
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.javacore.JCCorruptData;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaMethod;

public class JCJavaMethod implements JavaMethod {
	
	private final String fName;
	private final JavaClass fJavaClass;
	
	private String fSignature;
	private Vector fBytecodeSections;
	private Vector fCompiledSections;
	
	public JCJavaMethod (String name, JCJavaClass javaClass) throws JCInvalidArgumentsException{
		if (name == null) {
			throw new JCInvalidArgumentsException("A method from a javacore must have a valid name.");
		}
		if (javaClass == null) {
			throw new JCInvalidArgumentsException("A method must have a valid declaring class.");
		}
		fName = name;
		fJavaClass = javaClass;
		
		fSignature = null;
		fBytecodeSections = new Vector();
		fCompiledSections = new Vector();
		javaClass.addMethod(this);
	}

	
	/**
	 * 
	 */
	public Iterator getBytecodeSections() {
		return fBytecodeSections.iterator();
	}

	
	/**
	 * 
	 */
	public Iterator getCompiledSections() {
		return fCompiledSections.iterator();
	}

	
	/**
	 * 
	 */
	public JavaClass getDeclaringClass() throws CorruptDataException, DataUnavailable {
		if (fJavaClass == null) {
			throw new DataUnavailable();
		}
		return fJavaClass;
	}

	
	/**
	 * 
	 */
	public int getModifiers() throws CorruptDataException {
		throw new CorruptDataException(new JCCorruptData(null));
	}

	
	/**
	 * 
	 */
	public String getName() throws CorruptDataException {
		if (fName == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fName;
	}

	
	/**
	 * 
	 */
	public String getSignature() throws CorruptDataException {
		if (fSignature == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fSignature;
	}

	public boolean equals(Object o) {
		if (o == null) return false;
		if (getClass() != o.getClass()) return false;
		JCJavaMethod jm2 = (JCJavaMethod)o;
		return fName.equals(jm2.fName) && fJavaClass.equals(jm2.fJavaClass);
	}
	
	public int hashCode() {
		return fName.hashCode() ^ fJavaClass.hashCode();
	}
	
	public String toString() {
		try {
			return getDeclaringClass().getName() + "." + getName();
		} catch (CorruptDataException e) {
			return super.toString();
		} catch (DataUnavailable e) {
			return super.toString();
		}
	}
}
